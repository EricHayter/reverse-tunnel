#pragma once

#include <netinet/in.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>

/* Read from a socket file descriptor into a buffer's span. Reads required
 * amount to fill the entire span */
std::optional<int> read_bytes(int socket_fd, std::span<std::byte> buffer);


/* Send the entire contents of the buffer over the given socket */
std::optional<int> send_bytes(int socket_fd, std::span<const std::byte> buffer);

std::string get_addr_string(const sockaddr_in& sock_addr);

/* Read a uint16 in network byte order from the front of a byte span, converting
 * it to host order. Avoids the alignment/aliasing UB of casting the buffer
 * pointer. */
std::uint16_t read_net_u16(std::span<const std::byte> bytes);
