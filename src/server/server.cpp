#include "server/server.h"
#include "common/definitions.h"
#include "common/sock_helper.h"

#include <cstring>
#include <array>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

Server::Server(std::size_t num_workers)
{
    for (std::size_t i{}; i < num_workers; ++i) {
        workers_m.push_back(std::jthread{ &Server::worker_func, this });
    }

    event_thread_m = std::jthread{ &Server::coordinator_func, this };

    // start listening on a thread for connections. Then make epoll here?
}


void Server::worker_func(std::stop_token stop_token)
{
    while (!stop_token.stop_requested()) {
        int active_fd = queue_m.pop();

        // handle the activity
    }
}

void Server::coordinator_func(std::stop_token stop_token)
{
    while (!stop_token.stop_requested()) {
        // DO epoll stuff
    }
}

Error Server::handle_handshake()
{
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0x00); // Keep this blocking for now.
    if (socket_fd == -1) {
        return Error{ .context = "Failed to create a receiving socket" };
    }

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          // use IPv4 for now...
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_flags = AI_PASSIVE;        // fill in my IP for me

    addrinfo *local_addr;
    int errc = getaddrinfo(NULL, SERVER_HANDSHAKE_PORTNUM.c_str(), &hints, &local_addr);
    if (errc != 0) {
        return Error{ .context = "Failed to get addrinfo for this computer" };
    }

    errc = bind(socket_fd, local_addr->ai_addr, local_addr->ai_addrlen);
    if (errc == -1) {
        return Error{ .context = "Failed to bind to address" };
    }

    constexpr int BACKLOG_COUNT = 20;
    errc = listen(socket_fd, BACKLOG_COUNT);
    if (errc == -1) {
        return Error{ .context = "Call to listen() failed" };
    }

    // this is going to be some sort of epoll thing
    // accept()
    sockaddr_storage addr;
    unsigned int addr_len;
    int conn_fd = accept(socket_fd, (sockaddr*)&addr, &addr_len);
    if (conn_fd == -1) {
        return Error{ .context = "Call to accept() failed" };
    }

    std::array<std::byte, 2 * sizeof(uint16_t)> buffer;

    uint16_t num_mappings{ 0 };
    bool read_success = read_bytes(conn_fd, std::span(buffer).subspan(0, sizeof(uint16_t)));
    if (!read_success) {
        return Error{ .context = "Failed to read number of mappings" };
    }
    num_mappings = ntohs(*(uint16_t*)buffer.data());

    uint16_t mappings_processed{ 0 };
    while (mappings_processed < num_mappings) {
        read_success = read_bytes(conn_fd, std::span(buffer).subspan(0, 2 * sizeof(uint16_t)));
        if (!read_success) {
            return Error{ .context = "Failed to read number of mappings" };
        }
        PortMapping port_mapping{
            .to = num_mappings = ntohs(*(uint16_t*)buffer.data()),
            .from = num_mappings = ntohs(*((uint16_t*)buffer.data() + 1)),
        };
        // mapping log stuff here...
        port_map_m[port_mapping.from] = port_mapping.to;
        mappings_processed++;
        std::cout << std::format("Received port mapping #%d:\n", mappings_processed);
        std::cout << std::format("to: %d\n" , port_mapping.to);
        std::cout << std::format("from: %d\n\n" , port_mapping.from);
    }


    return {};
}
