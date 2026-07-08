#pragma once

#include <cstdint>
#include <string>
#include <format>

constexpr std::string SERVER_LISTENING_PORTNUM = "1738";

using PortNum = std::uint16_t;

/* maps from-to relationship */
using PortMapping = std::pair<PortNum, PortNum>;

inline std::string to_string(PortMapping port_mapping)
{
    auto [from, to] = port_mapping;
    return std::format("{{ from: {}, to: {}}}", from, to);
}

