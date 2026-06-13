#pragma once

#include <string_view>
#include <vector>

struct PortMapping;

// Parses a comma-separated list of port mappings, e.g. "22:2222,80:8080".
// Throws std::runtime_error if any mapping is invalid.
std::vector<PortMapping> parse_mapping_list(std::string_view list);
