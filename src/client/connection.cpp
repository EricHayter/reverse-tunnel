#include "client/connection.h"

#include <cstring>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

std::optional<Error> connect(std::string_view addr_string, std::span<PortMapping> mappings) {
    // Initial connection setup (might not need get addr info?
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          // only going to use IPv4 for now..
    hints.ai_socktype = SOCK_STREAM;    // TCP

    addrinfo *servinfo;

    int res =  getaddrinfo(addr_string.data(),
        std::to_string(SERVER_HANDSHAKE_PORTNUM).data(),
        &hints,
        &servinfo
    );

    int socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (socket_fd == -1) {
        return Error{ .context = "Failed to create socket FD" };
    }

    if (connect(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        return Error{ .context = "Call to connect() failed" };
    };

    std::vector<std::byte> msg_buffer((1 + 2 * mappings.size()) * sizeof(int));
    int offset = 0;
    for (const PortMapping& mapping: mappings) {
        // send from
        // send
    }
}
