#pragma once

#include <expected>
#include <vector>
#include <thread>
#include <unordered_map>

#include <sys/socket.h>
#include <netinet/in.h>

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
    std::expected<std::pair<int, sockaddr_in>, Error> accept_connection(int listening_socket);

    static std::expected<int, Error> create_connection(const sockaddr_in& client_addr_info);

    /* Handles the initial message from the client indicating the desired
     * mappings, and creates listening sockets for ingress packets with
     * create_listening_socket */
    Error handle_client_init(int socket);

    /* Creates a socket add calls listen on it. Also subscribes for read EPOLLIN
     * events on the socket monitor */
    std::expected<int, Error> create_listening_socket(PortNum port_num);

    /* Special case of a listening socket since once the connection is
     * established it will contain mapping requests */
    int client_listener_socket_m;
    std::unordered_map<int, PortNum> listening_sockets_m;

    /* IMPORTANT!
     * socket monitor must be declared before the worker functions since they
     * pull form the monitor directly */
    SocketMonitor socket_monitor_m;

    // worker pool stuff
    std::vector<std::jthread> workers_m;
    void worker_func(const std::stop_token& stop_token);

    // socket fd address of the connected client
    std::unordered_map<int, sockaddr_in> client_sockets_m;

    /* for each of the forwarding ports keep track of the socked address of
     * the client that we are forwarding packets to */
    std::unordered_map<PortNum, sockaddr_in> client_socket_address_m;

    /* stores "from-to" mappings for TCP port tunnels */
    std::unordered_map<int, int> port_map_m;

    /* maps the in and out sockets that form the forwarding tunnel */
    std::unordered_map<int, int> inbound_to_outbound_m;
    std::unordered_map<int, int> outbound_to_inbound_m;

};
