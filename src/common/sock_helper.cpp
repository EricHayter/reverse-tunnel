#include "common/sock_helper.h"

#include <sys/socket.h>
#include <errno.h>

bool read_bytes(int socket_fd, std::span<std::byte> buffer)
{
    while (true) {
        int recv_size = recv(socket_fd, buffer.data(), buffer.size_bytes(), 0x00);
        // socket got closed
        if (recv_size == 0) return false;
        // error of some sort
        if (recv_size < 0) {
            // the read was interrupted for whatever reason
            if (errno == EINTR) continue;
            return false;
        }
        buffer = buffer.subspan(recv_size);
    }
    return true;
}
