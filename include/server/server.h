#pragma once

#include <expected>
#include <vector>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <sys/socket.h>

#include "common/definitions.h"
#include "server/socket_monitor.h"

class Server {
public:
    explicit Server(std::size_t num_workers);
private:
    /* helper function to determine if a socket is a passive socket (i.e.
     * listen has been called on it */
    static bool is_listening_socket(int file_descriptor);

    /* Handles all of the read events on sockets monitored by the socket
     * monitor */
    void handle_read_event(int socket_fd);

    /* Handles a read event on a passive socket (i.e. an connection request)
     * subscribes the new connection to EPOLL_IN events */
    std::expected<int, Error> accept_connection(int listener_socket_fd);

    /* Handles the initial message from the client indicating the desired
     * mappings, and creates listening sockets for ingress packets with
     * create_listening_socket */
    Error handle_client_init(int socket_fd);

    /* Creates a socket add calls listen on it. Also subscribes for read EPOLLIN
     * events on the socket monitor */
    std::expected<int, Error> create_listening_socket(PortNum port_num);


    /* IMPORTANT!
     * socket monitor must be declared before the worker functions since they
     * pull form the monitor directly */
    SocketMonitor sock_monitor_m;

    // worker pool stuff
    std::vector<std::jthread> workers_m;
    void worker_func(const std::stop_token& stop_token);

    // special case when handling here (since this is where our management occurs I guess)
    int client_listener_sock_fd_m; // TODO might not need this
    std::unordered_set<int> client_socks_fd_m; // this should realistically be a vector

    // stores from-to port mappings
    std::unordered_map<int, int> port_map_m;
};
