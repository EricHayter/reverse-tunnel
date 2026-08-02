#include "common/sock_helper.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>

#include "spdlog/spdlog.h"

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

std::expected<int, Error> create_listening_socket(PortNum port_num) {
    int listening_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0x00);
    if (listening_socket == -1) {
        return std::unexpected<Error>{{.context = "Failed to create a receiving socket"}};
    }

    addrinfo hints{};
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // use IPv4 for now...
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    addrinfo *local_addr{};
    int errc = getaddrinfo(nullptr, std::to_string(port_num).c_str(), &hints, &local_addr);
    if (errc != 0) {
        return std::unexpected<Error>{{.context = "Failed to get addrinfo for this computer"}};
    }

    errc = bind(listening_socket, local_addr->ai_addr, local_addr->ai_addrlen);
    if (errc == -1) {
        freeaddrinfo(local_addr);
        return std::unexpected<Error>{{.context = "Failed to bind to address"}};
    }

    constexpr int BACKLOG_COUNT = 20;
    errc = listen(listening_socket, BACKLOG_COUNT);
    if (errc == -1) {
        freeaddrinfo(local_addr);
        return std::unexpected<Error>{{.context = "Call to listen() failed"}};
    }

    spdlog::info("Listening for external connections on {} (socket fd {})",
                 get_addr_string(*local_addr), listening_socket);

    freeaddrinfo(local_addr);
    return listening_socket;
}

std::expected<int, Error> create_connection(const sockaddr_in &addr) {
    int new_socket = socket(AF_INET, SOCK_STREAM, 0x00);
    if (new_socket == -1) {
        return std::unexpected(
            Error{.context = std::format("Failed to create a socket: {}",
                                         strerror(errno))}); // NOLINT(concurrency-mt-unsafe)
    }

    spdlog::debug("Attempting to connect to {}", get_addr_string(addr));

    if (connect(new_socket, reinterpret_cast<const sockaddr *>(&addr),
                sizeof(addr)) == -1) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        return std::unexpected(
            Error{.context = std::format("Connection attempt failed: {}",
                                         strerror(errno))}); // NOLINT(concurrency-mt-unsafe)
    }

    int flags = fcntl(new_socket, F_GETFL, 0);
    assert(flags != -1);
    assert(fcntl(new_socket, F_SETFL, flags | O_NONBLOCK) != -1);

    spdlog::debug("Successfully connected to {}", get_addr_string(addr));

    return new_socket;
}

std::expected<std::pair<int, sockaddr_in>, Error> accept_connection(int listening_socket) {
    sockaddr_in socket_address{};
    socklen_t addr_len{sizeof(socket_address)};

    int new_socket =
        accept4(listening_socket, reinterpret_cast<sockaddr *>(&socket_address), &addr_len,
                SOCK_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (new_socket == -1) {
        return std::unexpected{Error{.context = "Call to accept() failed"}};
    }

    spdlog::debug("Accepted connection from {} (socket fd {})", get_addr_string(socket_address),
                  new_socket);

    return std::pair{new_socket, socket_address};
}

std::uint16_t read_net_u16(std::span<const std::byte> bytes) {
    std::uint16_t net{};
    std::memcpy(&net, bytes.data(), sizeof(net));
    return ntohs(net);
}
