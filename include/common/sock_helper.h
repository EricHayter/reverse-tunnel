#pragma once

#include <netinet/in.h>

#include <optional>
#include <span>
#include <string>

/* Read from a socket file descriptor into a buffer's span. Reads required
 * amount to fill the entire span */
std::optional<int> read_bytes(int socket_fd, std::span<std::byte> buffer);


/* Send the entire contents of the buffer over the given socket */
std::optional<int> send_bytes(int socket_fd, std::span<const std::byte> buffer);

std::string get_addr_string(const sockaddr_in& sock_addr);
