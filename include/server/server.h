#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <list>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

#include <netinet/in.h>

#include "common/definitions.h"
#include "common/error.h"
#include "server/socket_monitor.h"

class Server {
  public:
    explicit Server(std::size_t num_workers);

  private:
    /* A one-way byte stream from a source socket to a sink socket: data read
     * from the source is buffered and drained out of the sink. A full-duplex
     * tunnel is just a pair of these pointing opposite ways */
    struct Pipe {
        static constexpr int BUFFER_SIZE = 1024;

        int source_socket;
        int sink_socket;
        std::array<std::byte, BUFFER_SIZE> buffer{};
        std::span<std::byte> message;
    };

    /* Handles all of the read events on sockets monitored by the socket
     * monitor */
    void handle_read_event(int socket);

    void attempt_forward_message(int recv_socket);

    /* Handles the initial message from the client indicating the desired
     * mappings, and opens listening sockets for ingress packets with
     * open_listener */
    Error handle_client_mapping_message(int client_socket);

    /* Opens a listening socket on the given port, records it, and arms it for
     * read events on the socket monitor */
    std::expected<int, Error> open_listener(PortNum port_num);

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
    void worker_func(const std::stop_token &stop_token);

    // socket fd address of the connected client
    std::unordered_map<int, sockaddr_in> client_sockets_m;

    /* Maps from ingress port to destination address */
    std::unordered_map<PortNum, sockaddr_in> ingress_port_to_destination_m;

    /* Handles a write event: the socket became writable, so drain whatever is
     * still pending to be sent out of it */
    void handle_write_event(int writable_socket);

    /* Sends the pipe's pending message out of its sink. On EAGAIN it arms the
     * sink for writability and returns (leaving the remainder for a later
     * flush); once fully sent it re-arms the source for reading */
    void flush_pipe(Pipe &pipe);

    std::list<Pipe> pipes_m;

    /* An fd's read events go to the pipe it is the source of; its write events
     * go to the pipe it is the sink of */
    std::unordered_map<int, std::list<Pipe>::iterator> source_pipes_m;
    std::unordered_map<int, std::list<Pipe>::iterator> sink_pipes_m;
};
