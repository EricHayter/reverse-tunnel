#pragma once

#include <expected>
#include <vector>
#include <thread>
#include <unordered_map>
#include <list>

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

    void attempt_forward_message(int recv_socket);

    /* Handles a read event on a passive socket (i.e. an connection request)
     * subscribes the new connection to EPOLL_IN events */
    std::expected<std::pair<int, sockaddr_in>, Error> accept_connection(int listening_socket);

    static std::expected<int, Error> create_connection(const sockaddr_in& client_addr_info);

    /* Handles the initial message from the client indicating the desired
     * mappings, and creates listening sockets for ingress packets with
     * create_listening_socket */
    Error handle_client_mapping_message(int client_socket);

    /* Creates a socket add calls listen on it. Also subscribes for read EPOLLIN
     * events on the socket monitor */
    std::expected<int, Error> create_listening_socket(PortNum port_num);

    /* Special case of a listening socket since once the connection is
     * established it will contain mapping requests */
    int client_listener_socket_m;

    /* Maps from listening socket to it's binded port number */
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

    /* Maps from ingress port to destination address */
    std::unordered_map<PortNum, sockaddr_in> ingress_port_to_destination_m;

    struct Connection {
        static constexpr int BUFFER_SIZE = 1024;
        Connection(int ingress_socket, int egress_socket) : ingress_socket(ingress_socket), egress_socket(egress_socket) {}
        int ingress_socket;
        std::array<std::byte, BUFFER_SIZE> ingress_buffer;
        int egress_socket;
        std::array<std::byte, BUFFER_SIZE> egress_buffer;
    };

    std::list<Connection> connections_m;
    std::unordered_map<int, std::list<Connection>::iterator> socket_to_conn_m;
};
