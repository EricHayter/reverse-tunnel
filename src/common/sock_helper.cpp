#include "common/sock_helper.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

std::optional<int> read_bytes(int socket_fd, std::span<std::byte> buffer)
{
    while (true) {
        int recv_size = recv(socket_fd, buffer.data(), buffer.size_bytes(), 0x00);
        // socket got closed
        if (recv_size == 0) return 0;
        // error of some sort
        if (recv_size < 0) {
            // the read was interrupted for whatever reason
            if (errno == EINTR) continue;
            return errno;
        }
        buffer = buffer.subspan(recv_size);
        if (buffer.empty())
            return {};
    }
}

std::optional<int> send_bytes(int socket_fd, std::span<const std::byte> buffer)
{
    while (true) {
        int send_size = send(socket_fd, buffer.data(), buffer.size_bytes(), 0x00);
        // socket got closed
        if (send_size == 0) return 0;
        // error of some sort
        if (send_size < 0) {
            // the read was interrupted for whatever reason
            if (errno == EINTR) continue;
            return errno;
        }
        buffer = buffer.subspan(send_size);
        if (buffer.empty())
            return {};
    }
}

std::string get_addr_string(const sockaddr_in& sock_addr)
{
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sock_addr.sin_addr), str, INET_ADDRSTRLEN);
    return str;
}
