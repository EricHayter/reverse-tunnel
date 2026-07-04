#pragma once

#include <string_view>
#include <span>
#include <optional>
#include <unordered_map>

#include "common/definitions.h"
#include "common/error.h"

class Client {
public:
    Client(std::string_view addr_string, std::span<const PortMapping> mappings);
    std::optional<Error> send_mapping_message();

private:
    int sock_fd_m{ };
    std::unordered_map<PortNum, PortNum> mappings_m;
};
