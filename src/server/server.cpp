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

    auto listener_socket_expected = init_listening_socket();
    if (!listener_socket_expected) {
        throw std::runtime_error(listener_socket_expected.error().context);
    }

    listener_sock_fd_m = *listener_socket_expected;
    auto listener_subscribe_failure = sock_monitor_m.subscribe(SocketMonitor::create_listener_event(listener_sock_fd_m));
    if (listener_subscribe_failure) {
        throw std::runtime_error(listener_socket_expected.error().context);
    }
    spdlog::debug("Server listener socket established");
}


// TODO this could really be abstracted waay entirely and it might make sense to sub too
std::expected<int, Error> Server::init_listening_socket()
{
    int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0x00);
    if (socket_fd == -1) {
        return std::unexpected<Error>{{ .context = "Failed to create a receiving socket" }};
    }

    addrinfo hints{};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          // use IPv4 for now...
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_flags = AI_PASSIVE;        // fill in my IP for me

    addrinfo *local_addr{};
    int errc = getaddrinfo(nullptr, SERVER_LISTENING_PORTNUM.c_str(), &hints, &local_addr);
    if (errc != 0) {
        return std::unexpected<Error>{{ .context = "Failed to get addrinfo for this computer" }};
    }

    errc = bind(socket_fd, local_addr->ai_addr, local_addr->ai_addrlen);
    if (errc == -1) {
        return std::unexpected<Error>{{ .context = "Failed to bind to address" }};
    }

    constexpr int BACKLOG_COUNT = 20;
    errc = listen(socket_fd, BACKLOG_COUNT);
    if (errc == -1) {
        return std::unexpected<Error>{{ .context = "Call to listen() failed" }};
    }

    sockaddr_in local_in{};
    std::memcpy(&local_in, local_addr->ai_addr, sizeof(local_in));
    spdlog::info("Tunnel server ready, listening for client connections on {}:{}",
            get_addr_string(local_in),
            SERVER_LISTENING_PORTNUM);

    return socket_fd;
}


void Server::worker_func(const std::stop_token& stop_token)
{
    while (!stop_token.stop_requested()) {
        auto event = sock_monitor_m.pull_event(stop_token);
        if (event) {
            int active_fd = event->data.fd;
            switch (event->events) {
            case EPOLLIN: {
                spdlog::debug("Handling read event on fd {}", active_fd);
                handle_read_event(event->data.fd);
                break;
            }
            case EPOLLHUP: {
                spdlog::debug("Handling hang-up event on fd {}", active_fd);
                break;
            }
            case EPOLLERR: {
                spdlog::debug("Handling error event on fd {}", active_fd);
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

void Server::handle_read_event(int socket_fd)
{
    /* special case need to handle handshake on the establishing of connection */
    if (socket_fd == listener_sock_fd_m) {
        handle_handshake();
        return;
    }

    int sending_socket = socket_map_m[socket_fd];
    constexpr int MSG_BUFFER_SIZE = 4096;
    std::array<std::byte, MSG_BUFFER_SIZE> msg_buffer{};
    ssize_t num_bytes = recv(socket_fd, msg_buffer.data(), msg_buffer.size(), 0x00);
    assert(num_bytes != -1);

    // TODO get some better error handling here
    send_bytes(sending_socket, std::span<std::byte>{ msg_buffer.data(), static_cast<std::size_t>(num_bytes) });
}

Error Server::handle_handshake()
{
    sockaddr_storage addr{};
    unsigned int addr_len{ sizeof(addr) };

    int conn_fd = accept(listener_sock_fd_m, reinterpret_cast<sockaddr*>(&addr), &addr_len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (conn_fd == -1) {
        return Error{ .context = "Call to accept() failed" };
    }

    spdlog::debug("Accepted connection from {}",
            get_addr_string(*reinterpret_cast<sockaddr_in*>(&addr)));

    std::array<std::byte, 2 * sizeof(uint16_t)> buffer{};

    uint16_t num_mappings{ 0 };
    auto read_errc = read_bytes(conn_fd, std::span{ buffer.data(), sizeof(PortNum) } );
    if (read_errc) {
        return Error{ .context = "Failed to read number of mappings" };
    }
    num_mappings = read_net_u16(buffer);

    spdlog::debug("Client provided {} mapping(s)", num_mappings);

    uint16_t mappings_processed{ 0 };
    while (mappings_processed < num_mappings) {
        read_errc = read_bytes(conn_fd, std::span{ buffer.data(), 2 * sizeof(uint16_t) });
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

        // need to create sockets here as well...
    }


    return {};
}
