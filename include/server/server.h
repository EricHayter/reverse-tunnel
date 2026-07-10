#pragma once

#include <expected>
#include <vector>
#include <thread>
#include <unordered_map>

#include <sys/socket.h>

#include "server/socket_monitor.h"

class Server {
public:
    Server(std::size_t num_workers);
private:
    std::expected<int, Error> init_listening_socket();

    SocketMonitor sock_monitor_m;

    // special case when handling here (since this is where our management occurs I guess)
    int listener_sock_fd_m;



    // client address (i.e. the computer that initiates the connection)
    sockaddr_storage client_addr_m{};
    unsigned int client_addr_len_m{};

    // worker pool stuff
    std::vector<std::jthread> workers_m;
    void worker_func(const std::stop_token& stop_token);
    Error handle_handshake();
    void handle_read_event(int socket_fd);

    // stores from-to mapping
    std::unordered_map<int, int> port_map_m;

    // TODO this need a much better name
    std::unordered_map<int, int> socket_map_m;
};

// does this even work for single threaded CPUs? Problem: wasting too much
// time on the EPOLL thread without pre-emption
// SOLUTION: I think I can use std::yield? Might need to test if this is
// actually a valid solution
// then use hardware_concurrency
