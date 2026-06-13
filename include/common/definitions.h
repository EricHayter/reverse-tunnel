#pragma once

#include <cstdint>

using PortNum = std::uint16_t;

struct PortMapping {
    PortNum to;
    PortNum from;
};
