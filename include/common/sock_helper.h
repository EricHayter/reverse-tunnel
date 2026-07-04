#pragma once

#include <span>

/* Read from a socket file descriptor into a buffer's span. Reads required
 * amount to fill the entire span */
bool read_bytes(int socket_fd, std::span<std::byte> buffer);
