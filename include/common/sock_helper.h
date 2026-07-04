#pragma once

#include <optional>
#include <span>

/* Read from a socket file descriptor into a buffer's span. Reads required
 * amount to fill the entire span */
bool read_bytes(int socket_fd, std::span<std::byte> buffer);


/* Send the entire contents of the buffer over the given socket */
std::optional<int> send_bytes(int socket_fd, std::span<const std::byte> buffer);
