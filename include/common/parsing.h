#pragma once

#include <string_view>
#include <optional>

#include "common/definitions.h"

std::optional<PortNum> parse_port_num(std::string_view port_string);

// parses a string of the format of <from port number>:<to port number> and
// parses a port mapping. Handles leading and trailing whitespace in
// port_mapping_string. In the case of parse failures return std::null_opt.
std::optional<PortMapping> parse_port_mapping(std::string_view port_mapping_string);
