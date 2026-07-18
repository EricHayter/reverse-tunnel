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
    /* Handles all of the read events on sockets monitored by the socket
     * monitor */
    void handle_read_event(int socket);

    /* Handles a read event on a passive socket (i.e. an connection request)
     * subscribes the new connection to EPOLL_IN events */
    std::expected<int, Error> accept_connection(int listening_socket);

    /* Handles the initial message from the client indicating the desired
     * mappings, and creates listening sockets for ingress packets with
     * create_listening_socket */
    Error handle_client_init(int socket);

    /* Creates a socket add calls listen on it. Also subscribes for read EPOLLIN
     * events on the socket monitor */
    std::expected<int, Error> create_listening_socket(PortNum port_num);


    /**/
    int client_listener_socket_m;
    std::unordered_set<int> listening_sockets_m;

    /* IMPORTANT!
     * socket monitor must be declared before the worker functions since they
     * pull form the monitor directly */
    SocketMonitor socket_monitor_m;

    // worker pool stuff
    std::vector<std::jthread> workers_m;
    void worker_func(const std::stop_token& stop_token);

    // special case when handling here (since this is where our management occurs I guess)
    std::unordered_set<int> client_socks_fd_m; // this should realistically be a vector

    // stores from-to port mappings
    std::unordered_map<int, int> port_map_m;

};
