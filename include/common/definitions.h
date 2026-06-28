#pragma once

#include <cstdint>
#include <string>

constexpr std::string SERVER_HANDSHAKE_PORTNUM = "1738";

using PortNum = std::uint16_t;

struct PortMapping {
    PortNum to;
    PortNum from;
};
