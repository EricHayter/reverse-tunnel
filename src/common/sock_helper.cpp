#include "common/sock_helper.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>

std::optional<int> read_bytes(int socket_fd, std::span<std::byte> buffer) {
    while (true) {
        const ssize_t recv_size = recv(socket_fd, buffer.data(), buffer.size_bytes(), 0x00);
        // socket got closed
        if (recv_size == 0) {
            return 0;
        }
        // error of some sort
        if (recv_size < 0) {
            // the read was interrupted for whatever reason
            if (errno == EINTR) {
                continue;
            }
            return errno;
        }
        buffer = buffer.subspan(recv_size);
        if (buffer.empty()) {
            return {};
        }
    }
}

std::optional<int> send_bytes(int socket_fd, std::span<const std::byte> buffer) {
    while (true) {
        const ssize_t send_size = send(socket_fd, buffer.data(), buffer.size_bytes(), 0x00);
        // socket got closed
        if (send_size == 0) {
            return 0;
        }
        // error of some sort
        if (send_size < 0) {
            // the read was interrupted for whatever reason
            if (errno == EINTR) {
                continue;
            }
            return errno;
        }
        buffer = buffer.subspan(send_size);
        if (buffer.empty()) {
            return {};
        }
    }
}

std::string get_addr_string(const sockaddr_in &sock_addr) {
    std::array<char, INET_ADDRSTRLEN> str{};
    inet_ntop(AF_INET, &(sock_addr.sin_addr), str.data(), INET_ADDRSTRLEN);
    return std::format("{}:{}", str.data(), ntohs(sock_addr.sin_port));
}

std::string get_addr_string(const addrinfo &info) {
    return get_addr_string(*reinterpret_cast<const sockaddr_in *>(
        info.ai_addr)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

std::uint16_t read_net_u16(std::span<const std::byte> bytes) {
    std::uint16_t net{};
    std::memcpy(&net, bytes.data(), sizeof(net));
    return ntohs(net);
}
