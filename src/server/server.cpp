#include "server/server.h"

#include <array>
#include <cstring>
#include <span>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "spdlog/spdlog.h"

#include "common/definitions.h"
#include "common/error.h"
#include "common/sock_helper.h"

Server::Server(std::size_t num_workers) {
    workers_m.reserve(num_workers);
    for (std::size_t i{}; i < num_workers; ++i) {
        workers_m.emplace_back(&Server::worker_func, this);
    }

    auto listener_socket_expected =
        create_listening_socket(SERVER_LISTENING_PORTNUM);
    if (!listener_socket_expected) {
        throw std::runtime_error(listener_socket_expected.error().context);
    }
    client_listener_socket_m = *listener_socket_expected;
}

std::expected<int, Error> Server::create_listening_socket(PortNum port_num) {
    int listening_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0x00);
    if (listening_socket == -1) {
        return std::unexpected<Error>{
            {.context = "Failed to create a receiving socket"}};
    }

    addrinfo hints{};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // use IPv4 for now...
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    addrinfo *local_addr{};
    int errc = getaddrinfo(nullptr, std::to_string(port_num).c_str(), &hints,
                           &local_addr);
    if (errc != 0) {
        return std::unexpected<Error>{
            {.context = "Failed to get addrinfo for this computer"}};
    }

    errc = bind(listening_socket, local_addr->ai_addr, local_addr->ai_addrlen);
    if (errc == -1) {
        freeaddrinfo(local_addr);
        return std::unexpected<Error>{{.context = "Failed to bind to address"}};
    }

    constexpr int BACKLOG_COUNT = 20;
    errc = listen(listening_socket, BACKLOG_COUNT);
    if (errc == -1) {
        freeaddrinfo(local_addr);
        return std::unexpected<Error>{{.context = "Call to listen() failed"}};
    }

    listening_sockets_m[listening_socket] = port_num;

    socket_monitor_m.arm(listening_socket, SocketMonitor::Interest::Read);

    spdlog::info("Listening for external connections on {} (socket fd {})",
                 get_addr_string(*local_addr), listening_socket);

    freeaddrinfo(local_addr);
    return listening_socket;
}

void Server::worker_func(const std::stop_token &stop_token) {
    // TODO this could be fixed in theory... Since these aren't enums... it's a
    // bitfield
    while (!stop_token.stop_requested()) {
        auto event = socket_monitor_m.pull_event(stop_token);
        if (event) {
            auto [socket_fd, socket_event] = *event;
            switch (socket_event) {
            case SocketMonitor::Event::Read: {
                spdlog::debug("Handling read event on fd {}", socket_fd);
                handle_read_event(socket_fd);
                break;
            }
            case SocketMonitor::Event::Write: {
                spdlog::debug("Handling write event on fd {}", socket_fd);
                break;
            }
            case SocketMonitor::Event::HangUp: {
                spdlog::debug("Handling hang-up event on fd {}", socket_fd);
                break;
            }
            case SocketMonitor::Event::Error: {
                spdlog::debug("Handling error event on fd {}", socket_fd);
                break;
            }
            }
        }
    }
}

void Server::handle_read_event(int socket) {
    /* receiving message from clients (message mapping)  */
    if (client_sockets_m.contains(socket)) {
        handle_client_mapping_message(socket);
    } else if (listening_sockets_m.contains(socket)) {
        /* new connection attempt */
        auto new_connection = accept_connection(socket);
        if (!new_connection) {
            spdlog::error("Connection acceptance failed on socket fd {}",
                          socket);
            return;
        }

        const auto &[ingress_socket, addr] = *new_connection;

        /* special case: this is a client connection, we should make note of
         * these so we can expect a handshake message, see case above */
        if (socket == client_listener_socket_m) {
            client_sockets_m[ingress_socket] = addr;
        } else {
            PortNum ingress_port = listening_sockets_m.at(socket);
            const sockaddr_in &peer_addr =
                ingress_port_to_destination_m.at(ingress_port);
            auto egress_socket = create_connection(peer_addr);
            if (!egress_socket) {
                spdlog::error(egress_socket.error().context);
                close(ingress_socket);
                return;
            }
            socket_monitor_m.arm(*egress_socket, SocketMonitor::Interest::Read);
            auto connection_iter = connections_m.emplace(
                connections_m.end(), ingress_socket, *egress_socket);
            socket_to_conn_m[ingress_socket] = connection_iter;
            socket_to_conn_m[*egress_socket] = connection_iter;
        }
    } else {
        /* received a regular message to forward */
        attempt_forward_message(socket);
    }
}

void Server::attempt_forward_message(int recv_socket) {
    Connection &conn = *socket_to_conn_m[recv_socket];
    assert(recv_socket == conn.ingress_socket ||
           recv_socket == conn.egress_socket);
    bool is_ingress_socket{recv_socket == conn.ingress_socket};
    std::span<std::byte> buffer_span =
        is_ingress_socket ? conn.ingress_buffer : conn.egress_buffer;

    ssize_t bytes_received =
        recv(recv_socket, buffer_span.data(), buffer_span.size(), 0x00);
    if (bytes_received == -1) {
        spdlog::error("Failed to forward message from socket {}: {}",
                      recv_socket,
                      strerror(errno)); // NOLINT(concurrency-mt-unsafe)
        return;
    }
    buffer_span = buffer_span.subspan(0, bytes_received);

    /* We need to send the message out of the opposite socket in the connection
     */
    int sending_socket =
        is_ingress_socket ? conn.egress_socket : conn.ingress_socket;
    ssize_t bytes_sent =
        send(sending_socket, buffer_span.data(), bytes_received, 0x00);
    while (buffer_span.size() > 0) {
        bytes_sent =
            send(sending_socket, buffer_span.data(), buffer_span.size(), 0x00);
        buffer_span = buffer_span.subspan(bytes_sent);
    }

    spdlog::debug("Forwarded message of {} bytes on fd {}", bytes_received,
                  recv_socket);

    /* receiver of the message isn't ready to take that new mesasge so we
     * throttle the reader and wait for the writer to be ready to write again */
    if (bytes_sent == EAGAIN) {
        socket_monitor_m.disarm(recv_socket, SocketMonitor::Interest::Read);
        socket_monitor_m.arm(recv_socket, SocketMonitor::Interest::Write);
    }
}

Error Server::handle_client_mapping_message(int client_socket) {
    /* handle the mapping message
     *
     * going to be a uint16 indicating the number of mappings followed by
     * from-to uint16_t pairs indicating the port numbers of from-to mappings
     * */
    std::array<std::byte, 2 * sizeof(PortNum)> buffer{};

    uint16_t num_mappings{0};
    auto read_errc =
        read_bytes(client_socket, std::span{buffer.data(), sizeof(PortNum)});
    if (read_errc) {
        return Error{.context = "Failed to read number of mappings"};
    }
    num_mappings = read_net_u16(buffer);

    spdlog::debug("Client provided {} mapping(s)", num_mappings);

    uint16_t mappings_processed{0};
    while (mappings_processed < num_mappings) {
        read_errc = read_bytes(client_socket,
                               std::span{buffer.data(), 2 * sizeof(uint16_t)});
        if (read_errc) {
            return Error{.context = "Failed to read number of mappings"};
        }

        PortNum from_port =
            read_net_u16(std::span(buffer).subspan(0, sizeof(uint16_t)));
        PortNum to_port = read_net_u16(
            std::span(buffer).subspan(sizeof(uint16_t), sizeof(uint16_t)));
        PortMapping port_mapping{from_port, to_port};
        spdlog::debug("Received port mapping #{}: {}", mappings_processed,
                      to_string(port_mapping));
        mappings_processed++;

        /* take the information that we stored from the client connection to
         * build the destination address for this mapping */
        sockaddr_in proxy_destination = client_sockets_m.at(client_socket);
        proxy_destination.sin_port = htons(to_port);
        ingress_port_to_destination_m[from_port] = proxy_destination;

        auto listening_socket = create_listening_socket(from_port);
        if (!listening_socket) {
            return listening_socket.error().with(std::format(
                "Failed to setup listening socket on port {}", from_port));
        }
    }
    return {};
}

std::expected<std::pair<int, sockaddr_in>, Error>
Server::accept_connection(int listening_socket) {
    sockaddr_in socket_address{};
    socklen_t addr_len{sizeof(socket_address)};

    int new_socket = accept(
        listening_socket, reinterpret_cast<sockaddr *>(&socket_address),
        &addr_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (new_socket == -1) {
        return std::unexpected{Error{.context = "Call to accept() failed"}};
    }

    spdlog::debug("Accepted connection from {} (socket fd {})",
                  get_addr_string(socket_address), new_socket);

    socket_monitor_m.arm(new_socket, SocketMonitor::Interest::Read);
    return std::pair{new_socket, socket_address};
}

std::expected<int, Error>
Server::create_connection(const sockaddr_in &client_addr_info) {
    int new_socket = socket(AF_INET, SOCK_STREAM, 0x00);
    if (new_socket == -1) {
        return std::unexpected(Error{
            .context =
                std::format("Failed to create a socket: {}",
                            strerror(errno))}); // NOLINT(concurrency-mt-unsafe)
    }

    spdlog::debug("Attempting to connect to {}",
                  get_addr_string(client_addr_info));

    if (connect(new_socket,
                reinterpret_cast<const sockaddr *>(&client_addr_info),
                sizeof(client_addr_info)) ==
        -1) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        return std::unexpected(Error{
            .context =
                std::format("Connection attempt failed: {}",
                            strerror(errno))}); // NOLINT(concurrency-mt-unsafe)
    }

    int flags = fcntl(new_socket, F_GETFL, 0);
    assert(flags != -1);
    assert(fcntl(new_socket, F_SETFL, flags | O_NONBLOCK) != -1);

    spdlog::debug("Successfully connected to {}",
                  get_addr_string(client_addr_info));

    return new_socket;
}
