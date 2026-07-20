#include "server/server.h"

#include <cstring>
#include <span>
#include <array>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "spdlog/spdlog.h"

#include "common/definitions.h"
#include "common/error.h"
#include "common/sock_helper.h"

Server::Server(std::size_t num_workers)
{
    workers_m.reserve(num_workers);
    for (std::size_t i{}; i < num_workers; ++i) {
        workers_m.emplace_back(&Server::worker_func, this);
    }

    auto listener_socket_expected = create_listening_socket(SERVER_LISTENING_PORTNUM);
    if (!listener_socket_expected) {
        throw std::runtime_error(listener_socket_expected.error().context);
    }
    client_listener_socket_m = *listener_socket_expected;
}


std::expected<int, Error> Server::create_listening_socket(PortNum port_num)
{
    int listening_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0x00);
    if (listening_socket == -1) {
        return std::unexpected<Error>{{ .context = "Failed to create a receiving socket" }};
    }

    addrinfo hints{};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          // use IPv4 for now...
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_flags = AI_PASSIVE;        // fill in my IP for me

    addrinfo *local_addr{};
    int errc = getaddrinfo(nullptr, std::to_string(port_num).c_str(), &hints, &local_addr);
    if (errc != 0) {
        return std::unexpected<Error>{{ .context = "Failed to get addrinfo for this computer" }};
    }

    errc = bind(listening_socket, local_addr->ai_addr, local_addr->ai_addrlen);
    if (errc == -1) {
        freeaddrinfo(local_addr);
        return std::unexpected<Error>{{ .context = "Failed to bind to address" }};
    }

    constexpr int BACKLOG_COUNT = 20;
    errc = listen(listening_socket, BACKLOG_COUNT);
    if (errc == -1) {
        freeaddrinfo(local_addr);
        return std::unexpected<Error>{{ .context = "Call to listen() failed" }};
    }

    listening_sockets_m[listening_socket] = port_num;

    auto listener_subscribe_failure = socket_monitor_m.subscribe_reader(listening_socket);
    if (listener_subscribe_failure) {
        freeaddrinfo(local_addr);
        return std::unexpected(*listener_subscribe_failure);
    }

    spdlog::info("Listening for external connections on {}:{} (socket fd {})",
            get_addr_string(*reinterpret_cast<sockaddr_in*>(local_addr->ai_addr)), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            port_num,
            listening_socket
            );

    freeaddrinfo(local_addr);
    return listening_socket;
}


void Server::worker_func(const std::stop_token& stop_token)
{
    // TODO this could be fixed in theory... Since these aren't enums... it's a bitfield
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

void Server::handle_read_event(int socket)
{
    /* receiving message from clients (message mapping)  */
    if (client_sockets_m.contains(socket)) {
        handle_client_init(socket);
        return;
    }

    /* new connection connection events */
    if (listening_sockets_m.contains(socket)) {
        auto new_connection = accept_connection(socket);
        if (!new_connection) {
            spdlog::error("Connection acceptance failed on socket fd {}", socket);
            return;
        }

        const auto&[new_socket, addr] = *new_connection;

        /* special case: this is a client connection, we should make note of
         * these so we can expect a handshake message, see case above */
        if (socket == client_listener_socket_m) {
            client_sockets_m[new_socket] = addr;
        } else {
            PortNum to_port = listening_sockets_m.at(socket);
            const sockaddr_in& peer_addr = client_socket_address_m.at(to_port);
            auto outbound_socket = create_connection(peer_addr);
            if (!outbound_socket) {
                spdlog::error(outbound_socket.error().context);
                close(new_socket);
                return;
            }
            inbound_to_outbound_m[new_socket] = *outbound_socket;
            outbound_to_inbound_m[*outbound_socket] = new_socket;
        }
        return;
    }

    /* just a message that needs to be forwarded */
    constexpr int MSG_BUFFER_SIZE = 1024;
    std::array<std::byte, MSG_BUFFER_SIZE> msg_buffer{};
    ssize_t num_bytes{};
    num_bytes = recv(socket, msg_buffer.data(), msg_buffer.size(), 0x00);
    if (num_bytes == -1) {
        // TODO this should return an error I think?
        spdlog::error("recv call failed: {}", strerror(errno)); // NOLINT(concurrency-mt-unsafe)
    }
    assert(num_bytes != -1); // TODO this can fail for non assert types of stuff
    send_bytes(inbound_to_outbound_m[socket], std::span(msg_buffer).subspan(0, num_bytes));
}

Error Server::handle_client_init(int socket)
{
    /* handle the mapping message
     *
     * going to be a uint16 indicating the number of mappings followed by
     * from-to uint16_t pairs indicating the port numbers of from-to mappings
     * */
    std::array<std::byte, 2 * sizeof(PortNum)> buffer{};

    uint16_t num_mappings{ 0 };
    auto read_errc = read_bytes(socket, std::span{ buffer.data(), sizeof(PortNum) } );
    if (read_errc) {
        return Error{ .context = "Failed to read number of mappings" };
    }
    num_mappings = read_net_u16(buffer);

    spdlog::debug("Client provided {} mapping(s)", num_mappings);

    uint16_t mappings_processed{ 0 };
    while (mappings_processed < num_mappings) {
        read_errc = read_bytes(socket, std::span{ buffer.data(), 2 * sizeof(uint16_t) });
        if (read_errc) {
            return Error{ .context = "Failed to read number of mappings" };
        }

        PortNum from_port = read_net_u16(std::span(buffer).subspan(0, sizeof(uint16_t)));
        PortNum to_port = read_net_u16(std::span(buffer).subspan(sizeof(uint16_t), sizeof(uint16_t)));
        PortMapping port_mapping{ from_port, to_port };

        port_map_m[from_port] = to_port;
        mappings_processed++;
        spdlog::debug("Received port mapping #{}: {}", mappings_processed, to_string(port_mapping));

        sockaddr_in data_addr = client_sockets_m.at(socket);
        data_addr.sin_port = htons(from_port);
        client_socket_address_m[to_port] = data_addr;

        auto listening_socket = create_listening_socket(to_port);
        if (!listening_socket) {
            return listening_socket.error().with(std::format("Failed to setup listening socket on port {}", to_port));
        }
    }
    return {};
}

std::expected<std::pair<int, sockaddr_in>, Error> Server::accept_connection(int listening_socket)
{
    sockaddr_in socket_address{};
    socklen_t addr_len{ sizeof(socket_address) };

    int new_socket = accept(listening_socket, reinterpret_cast<sockaddr*>(&socket_address), &addr_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (new_socket == -1) {
        return std::unexpected{Error{ .context = "Call to accept() failed" }};
    }

    spdlog::debug("Accepted connection from {} (socket fd {})",
            get_addr_string(socket_address),
            new_socket
            );

    if (auto err = socket_monitor_m.subscribe_reader(new_socket)) {
        return std::unexpected(*err);
    }
    return std::pair{ new_socket, socket_address };
}

std::expected<int, Error> Server::create_connection(const sockaddr_in& client_addr_info)
{
    int new_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0x00);
    if (new_socket == -1) {
        return std::unexpected(Error{ .context = std::format("Failed to create a socket: {}", strerror(errno)) }); // NOLINT(concurrency-mt-unsafe)
    }

    spdlog::debug("Attempting to connect to {}", get_addr_string(client_addr_info));

    if (connect(new_socket, reinterpret_cast<const sockaddr*>(&client_addr_info), sizeof(client_addr_info)) == -1) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        return std::unexpected(Error{ .context = std::format("Connection attempt failed: {}", strerror(errno)) }); // NOLINT(concurrency-mt-unsafe)
    }

    spdlog::debug("Successfully connected to {}", get_addr_string(client_addr_info));

    return new_socket;
}
