#pragma once

#include <cstdint>
#include <format>
#include <string>

using PortNum = std::uint16_t;

constexpr PortNum SERVER_LISTENING_PORTNUM = 1738;

/* maps from-to relationship */
using PortMapping = std::pair<PortNum, PortNum>;

inline std::string to_string(PortMapping port_mapping) {
    auto [from_port, to_port] = port_mapping;
    return std::format("{{ from: {}, to: {}}}", from_port, to_port);
}
