#pragma once

#include <netdb.h>
#include <netinet/in.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "common/definitions.h"
#include "common/error.h"

/* Read from a socket file descriptor into a buffer's span. Reads required
 * amount to fill the entire span */
std::optional<int> read_bytes(int socket_fd, std::span<std::byte> buffer);

/* Send the entire contents of the buffer over the given socket */
std::optional<int> send_bytes(int socket_fd, std::span<const std::byte> buffer);

std::string get_addr_string(const sockaddr_in &sock_addr);

/* Overload for an addrinfo. Keeps the sockaddr -> sockaddr_in reinterpret_cast
 * in a single place so callers do not have to spell it out */
std::string get_addr_string(const addrinfo &info);

/* Creates a TCP socket bound to the given port and puts it in the listening
 * state, returning the socket fd. The returned socket is non-blocking */
std::expected<int, Error> create_listening_socket(PortNum port_num);

/* Creates a TCP socket and connects it to the given address, returning the
 * connected socket fd. The returned socket is non-blocking */
std::expected<int, Error> create_connection(const sockaddr_in &addr);

/* Accepts a pending connection on a listening socket, returning the accepted
 * socket fd and the peer's address. The returned socket is non-blocking */
std::expected<std::pair<int, sockaddr_in>, Error> accept_connection(int listening_socket);

/* Read a uint16 in network byte order from the front of a byte span, converting
 * it to host order. Avoids the alignment/aliasing UB of casting the buffer
 * pointer. */
std::uint16_t read_net_u16(std::span<const std::byte> bytes);
