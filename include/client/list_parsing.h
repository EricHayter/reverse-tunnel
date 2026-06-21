#pragma once

#include <expected>
#include <string_view>
#include <vector>

#include "common/error.h"

struct PortMapping;

// Parses a comma-separated list of port mappings, e.g. "22:2222,80:8080".
// Returns a TunnelErrc::invalid_mapping error if any mapping is invalid.
std::expected<std::vector<PortMapping>, Error> parse_mapping_list(std::string_view list);
