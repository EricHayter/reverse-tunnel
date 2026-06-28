#include "client/connection.h"

#include <cstring>
#include <format>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

std::optional<Error> connect(std::string_view addr_string, std::span<PortMapping> mappings) {
    // Initial connection setup (might not need get addr info?
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));  // NULL it out so we only select what we wnat
    hints.ai_family = AF_INET;              // only going to use IPv4 for now..
    hints.ai_socktype = SOCK_STREAM;        // TCP

    addrinfo *server_info;

    int res =  getaddrinfo(addr_string.data(),
        SERVER_HANDSHAKE_PORTNUM.data(),
        &hints,
        &server_info
    );

    if (res != 0 || server_info == nullptr) {
        return Error{ .context = "Call to getaddrinfo() failed (Likely bad hostname or service)" };
    }

    int socket_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_fd == -1) {
        return Error{ .context = "Failed to create socket FD" };
    }

    if (connect(socket_fd, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        return Error{ .context = "Call to connect() failed" };
    };

    auto err = send_mapping_message(socket_fd, mappings);
    if (err) {
        return Error{ .context = "Connection attempt with server failed" }.with(err->context);
    }
    return {};
}

std::optional<Error> send_mapping_message(int socket_fd, std::span<const PortMapping> mappings)
{
    int offset = 0;

    std::vector<std::byte> msg_buffer((1 + 2 * mappings.size()) * sizeof(int));

    uint16_t mapping_count = htons(static_cast<uint16_t>(mappings.size()));
    std::memcpy(msg_buffer.data() + offset, &mapping_count, sizeof(mapping_count));
    offset += sizeof(mapping_count);

    // serializing the mappings
    for (const PortMapping& mapping: mappings) {
        // send from
        uint16_t from_port = htons(mapping.from);
        std::memcpy(msg_buffer.data() + offset, &from_port, sizeof(from_port));
        offset += sizeof(from_port);

        // send to
        uint16_t to_port = htons(mapping.to);
        std::memcpy(msg_buffer.data() + offset, &to_port, sizeof(to_port));
        offset += sizeof(to_port);
    }

    int num_bytes_sent = 0;
    int sent_flags = 0x00;
    while (num_bytes_sent < msg_buffer.size()) {
        int errc = send(socket_fd, msg_buffer.data() + num_bytes_sent, msg_buffer.size() - num_bytes_sent, sent_flags);

        // check to see if the send failed
        if (errc == -1) {
            return Error{ .context = std::format("Failed to send data to server (errno %d)", errc) };
        }

        num_bytes_sent += errc;
    }

    return {};
}
