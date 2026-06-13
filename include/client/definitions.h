#pragma once

#include <string>
#include <vector>

#include "common/definitions.h"

struct ClientConfig {
    std::string ip_addr;
    std::vector<PortMapping> mappings;
};
