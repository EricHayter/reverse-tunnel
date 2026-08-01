#include "client/client.h"

#include <arpa/inet.h>
#include <cstring>
#include <format>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include "spdlog/spdlog.h"

#include "common/sock_helper.h"

Client::Client(const std::string &addr_string, std::span<const PortMapping> mappings)
    : mappings_m{mappings.begin(), mappings.end()} {
    // Initial connection setup (might not need get addr info?
    addrinfo hints{};
    std::memset(&hints, 0,
                sizeof(hints));      // NULL it out so we only select what we wnat
    hints.ai_family = AF_INET;       // only going to use IPv4 for now..
    hints.ai_socktype = SOCK_STREAM; // TCP

    addrinfo *server_info{};

    int res = getaddrinfo(addr_string.c_str(), std::to_string(SERVER_LISTENING_PORTNUM).c_str(),
                          &hints, &server_info);

    if (res != 0 || server_info == nullptr) {
        throw std::runtime_error("Call to getaddrinfo() failed (Likely bad hostname or service)");
    }

    sock_fd_m = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (sock_fd_m == -1) {
        throw std::runtime_error("Failed to create socket FD");
    }

    spdlog::debug("Attempting to connect at {}", get_addr_string(*server_info));

    if (connect(sock_fd_m, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        throw std::runtime_error("Call to connect() failed");
    };

    spdlog::debug("Successfully connected to {}, sending mapping message",
                  get_addr_string(*server_info));

    auto err = send_mapping_message();
    if (err) {
        throw std::runtime_error(
            Error{.context = "Connection attempt with server failed"}.with(err->context).context);
    }
}

std::optional<Error> Client::send_mapping_message() {
    int offset = 0;

    std::vector<std::byte> msg_buffer((1 + (2 * mappings_m.size())) * sizeof(PortNum));

    uint16_t mapping_count = htons(static_cast<uint16_t>(mappings_m.size()));
    std::memcpy(std::span(msg_buffer).subspan(offset).data(), &mapping_count,
                sizeof(mapping_count));
    offset += sizeof(mapping_count);

    // serializing the mappings_m
    for (const auto &[from_port, to_port] : mappings_m) {
        PortNum from_port_network = htons(from_port);
        std::memcpy(std::span(msg_buffer).subspan(offset).data(), &from_port_network,
                    sizeof(from_port_network));
        offset += sizeof(from_port);
        PortNum to_port_network = htons(to_port);
        std::memcpy(std::span(msg_buffer).subspan(offset).data(), &to_port_network,
                    sizeof(to_port_network));
        offset += sizeof(to_port);
    }

    // can use a helper here to send
    auto err_code = send_bytes(sock_fd_m, msg_buffer);
    if (err_code) {
        return Error{.context = std::format("Failed to send data to server (errno {})", *err_code)};
    }

    return {};
}
