#include "server/server.h"

#include <array>
#include <cstring>
#include <span>

#include <arpa/inet.h>
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

    auto listener_socket_expected = open_listener(SERVER_LISTENING_PORTNUM);
    if (!listener_socket_expected) {
        throw std::runtime_error(listener_socket_expected.error().context);
    }
    client_listener_socket_m = *listener_socket_expected;
}

std::expected<int, Error> Server::open_listener(PortNum port_num) {
    auto listening_socket = create_listening_socket(port_num);
    if (!listening_socket) {
        return listening_socket;
    }

    listening_sockets_m[*listening_socket] = port_num;
    socket_monitor_m.arm(*listening_socket, SocketMonitor::Interest::Read);
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
                handle_write_event(socket_fd);
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
            spdlog::error("Connection acceptance failed on socket fd {}", socket);
            return;
        }

        const auto &[ingress_socket, addr] = *new_connection;
        socket_monitor_m.arm(ingress_socket, SocketMonitor::Interest::Read);

        /* special case: this is a client connection, we should make note of
         * these so we can expect a handshake message, see case above */
        if (socket == client_listener_socket_m) {
            client_sockets_m[ingress_socket] = addr;
        } else {
            PortNum ingress_port = listening_sockets_m.at(socket);
            const sockaddr_in &peer_addr = ingress_port_to_destination_m.at(ingress_port);
            auto egress_socket = create_connection(peer_addr);
            if (!egress_socket) {
                spdlog::error(egress_socket.error().context);
                close(ingress_socket);
                return;
            }
            socket_monitor_m.arm(*egress_socket, SocketMonitor::Interest::Read);

            /* wire up a pipe for each direction and route each endpoint's read
             * events to the pipe that reads from it and its write events to the
             * pipe that drains to it */
            auto to_egress = pipes_m.emplace(pipes_m.end(), ingress_socket, *egress_socket);
            auto to_ingress = pipes_m.emplace(pipes_m.end(), *egress_socket, ingress_socket);
            source_pipes_m[ingress_socket] = to_egress;
            sink_pipes_m[*egress_socket] = to_egress;
            source_pipes_m[*egress_socket] = to_ingress;
            sink_pipes_m[ingress_socket] = to_ingress;
        }
    } else {
        /* received a regular message to forward */
        attempt_forward_message(socket);
    }
}

void Server::attempt_forward_message(int recv_socket) {
    Pipe &pipe = *source_pipes_m.at(recv_socket);

    ssize_t bytes = recv(recv_socket, pipe.buffer.data(), pipe.buffer.size(), 0);
    if (bytes == -1) {
        spdlog::error("Failed to read from socket {}: {}", recv_socket,
                      strerror(errno)); // NOLINT(concurrency-mt-unsafe)
        return;
    }
    pipe.message = std::span(pipe.buffer).subspan(0, static_cast<std::size_t>(bytes));
    spdlog::debug("Forwarding {} bytes from fd {} to fd {}", bytes, recv_socket, pipe.sink_socket);
    flush_pipe(pipe);
}

void Server::handle_write_event(int writable_socket) {
    flush_pipe(*sink_pipes_m.at(writable_socket));
}

void Server::flush_pipe(Pipe &pipe) {
    while (!pipe.message.empty()) {
        ssize_t bytes = send(pipe.sink_socket, pipe.message.data(), pipe.message.size(), 0);
        if (bytes == -1) {
            if (errno == EAGAIN) {
                /* sink is backed up; wait for it to become writable before
                 * sending more. The source stays un-armed for reading (oneshot
                 * already disarmed it) which applies backpressure */
                socket_monitor_m.arm(pipe.sink_socket, SocketMonitor::Interest::Write);
                return;
            }
            spdlog::error("Failed to send on socket {}: {}", pipe.sink_socket,
                          strerror(errno)); // NOLINT(concurrency-mt-unsafe)
            return;
        }
        pipe.message = pipe.message.subspan(static_cast<std::size_t>(bytes));
    }

    /* fully flushed, so resume reading from the source */
    socket_monitor_m.arm(pipe.source_socket, SocketMonitor::Interest::Read);
}

Error Server::handle_client_mapping_message(int client_socket) {
    /* handle the mapping message
     *
     * going to be a uint16 indicating the number of mappings followed by
     * from-to uint16_t pairs indicating the port numbers of from-to mappings
     * */
    std::array<std::byte, 2 * sizeof(PortNum)> buffer{};

    uint16_t num_mappings{0};
    auto read_errc = read_bytes(client_socket, std::span{buffer.data(), sizeof(PortNum)});
    if (read_errc) {
        return Error{.context = "Failed to read number of mappings"};
    }
    num_mappings = read_net_u16(buffer);

    spdlog::debug("Client provided {} mapping(s)", num_mappings);

    uint16_t mappings_processed{0};
    while (mappings_processed < num_mappings) {
        read_errc = read_bytes(client_socket, std::span{buffer.data(), 2 * sizeof(uint16_t)});
        if (read_errc) {
            return Error{.context = "Failed to read number of mappings"};
        }

        PortNum from_port = read_net_u16(std::span(buffer).subspan(0, sizeof(uint16_t)));
        PortNum to_port =
            read_net_u16(std::span(buffer).subspan(sizeof(uint16_t), sizeof(uint16_t)));
        PortMapping port_mapping{from_port, to_port};
        spdlog::debug("Received port mapping #{}: {}", mappings_processed, to_string(port_mapping));
        mappings_processed++;

        /* take the information that we stored from the client connection to
         * build the destination address for this mapping */
        sockaddr_in proxy_destination = client_sockets_m.at(client_socket);
        proxy_destination.sin_port = htons(to_port);
        ingress_port_to_destination_m[from_port] = proxy_destination;

        auto listening_socket = open_listener(from_port);
        if (!listening_socket) {
            return listening_socket.error().with(
                std::format("Failed to setup listening socket on port {}", from_port));
        }
    }
    return {};
}

