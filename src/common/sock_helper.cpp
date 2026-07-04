#include "common/sock_helper.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#include <iostream>
#include <format>

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

std::optional<int> send_bytes(int socket_fd, std::span<const std::byte> buffer)
{
    std::cout << std::format("starting to send {} bytes\n", buffer.size_bytes());
    while (true) {
        int send_size = send(socket_fd, buffer.data(), buffer.size_bytes(), 0x00);
        std::cout << std::format("sent {} bytes\n", send_size);
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
