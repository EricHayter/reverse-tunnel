#include "server/server.h"

#include <cstring>
#include <span>
#include <array>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "spdlog/spdlog.h"

#include "common/definitions.h"
#include "common/error.h"
#include "common/sock_helper.h"

Server::Server(std::size_t num_workers)
{
    workers_m.resize(num_workers);
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
        return std::unexpected<Error>{{ .context = "Failed to bind to address" }};
    }

    constexpr int BACKLOG_COUNT = 20;
    errc = listen(listening_socket, BACKLOG_COUNT);
    if (errc == -1) {
        return std::unexpected<Error>{{ .context = "Call to listen() failed" }};
    }

    listening_sockets_m.insert(listening_socket);

    // is this not out of order???? with the stuff above?
    auto listener_subscribe_failure = sock_monitor_m.subscribe(SocketMonitor::create_listener_event(listening_socket));
    if (listener_subscribe_failure) {
        return std::unexpected(*listener_subscribe_failure);
    }

    sockaddr_in local_in{};
    std::memcpy(&local_in, local_addr->ai_addr, sizeof(local_in));
    spdlog::info("Listening for external connections on {}:{} (socket fd {})",
            get_addr_string(local_in),
            port_num,
            listening_socket
            );


    return listening_socket;
}


void Server::worker_func(const std::stop_token& stop_token)
{
    // TODO this could be fixed in theory...
    while (!stop_token.stop_requested()) {
        auto event = sock_monitor_m.pull_event(stop_token);
        if (event) {
            int notified_socket = event->data.fd;
            switch (event->events) {
            case EPOLLIN: {
                spdlog::debug("Handling read event on fd {}", notified_socket);
                handle_read_event(event->data.fd);
                break;
            }
            case EPOLLHUP: {
                spdlog::debug("Handling hang-up event on fd {}", notified_socket);
                break;
            }
            case EPOLLERR: {
                spdlog::debug("Handling error event on fd {}", notified_socket);
                break;
            }
            default: {
                std::unreachable();
                assert(false);
            }
            }
        }
    }
}

void Server::handle_read_event(int socket)
{
    /* receiving message from clients (message mapping)  */
    if (client_socks_fd_m.contains(socket)) {
        handle_client_init(socket);
        return;
    }

    /* new connection connection events */
    if (listening_sockets_m.contains(socket)) {
        auto connection_result = accept_connection(socket);
        if (!connection_result) {
            spdlog::error("Connection acceptance failed on socket fd {}", socket);
            return;
        }

        /* special case: this is a client connection, we should make note of
         * these so we can expect a handshake message, see case above */
        if (socket == client_listener_socket_m) {
               client_socks_fd_m.insert(socket);
        }
        return;
    }


    /* just a message that needs to be forwarded */
    constexpr int MSG_BUFFER_SIZE = 4096;
    std::array<std::byte, MSG_BUFFER_SIZE> msg_buffer{};
    ssize_t num_bytes{};
    std::span<std::byte> data_span = std::span(msg_buffer).subspan(sizeof(socket) + sizeof(num_bytes));
    num_bytes = recv(socket, data_span.data(), data_span.size_bytes(), 0x00);
    if (num_bytes == -1) {
        // TODO this should return an error I think?
        spdlog::error("recv call failed: {}", strerror(errno)); // NOLINT(concurrency-mt-unsafe)
    }
    assert(num_bytes != -1); // TODO this can fail for non assert types of stuff

    std::memcpy(msg_buffer.data(), &socket, sizeof(socket));
    std::memcpy(std::span(msg_buffer).subspan(sizeof(socket)).data(), &num_bytes, sizeof(num_bytes));

    // TODO can't have this hardcoded to the client sock when we have multiple clients (it'll need the bijective mapping later)
    // send_bytes(client_sock_fd_m, std::span(msg_buffer).subspan(0, sizeof(socket_fd) + sizeof(num_bytes) + num_bytes));
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

        PortNum to_port = read_net_u16(std::span(buffer).subspan(0, sizeof(uint16_t)));
        PortNum from_port = read_net_u16(std::span(buffer).subspan(sizeof(uint16_t), sizeof(uint16_t)));
        PortMapping port_mapping{ from_port, to_port };

        // mapping log stuff here...
        port_map_m[from_port] = to_port;
        mappings_processed++;
        spdlog::debug("Received port mapping #{}: {}", mappings_processed, to_string(port_mapping));

        /* create the listening socket to accept egress connections */
        auto listening_socket = create_listening_socket(to_port);
        if (!listening_socket) {
            return listening_socket.error().with(std::format("Failed to setup listening socket on port {}", to_port));
        }
    }
    return {};
}

std::expected<int, Error> Server::accept_connection(int listening_socket)
{
    sockaddr_storage addr{};
    unsigned int addr_len{ sizeof(addr) };

    int new_socket = accept(listening_socket, reinterpret_cast<sockaddr*>(&addr), &addr_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (new_socket == -1) {
        return std::unexpected{Error{ .context = "Call to accept() failed" }};
    }

    spdlog::debug("Accepted connection from {} (socket fd {})",
            get_addr_string(*reinterpret_cast<sockaddr_in*>(&addr)), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            new_socket
            );

    // subscribe it and save it
    sock_monitor_m.subscribe(SocketMonitor::create_listener_event(new_socket));
    return new_socket;
}
