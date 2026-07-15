#include "common/parsing.h"

#include <charconv>

std::optional<PortNum> parse_port_num(std::string_view port_string) {
    PortNum result{};
    auto [ptr, ec] = std::from_chars(port_string.begin(), port_string.end(), result);
    if (ec != std::errc{} || ptr != port_string.end()) {
        return {};
    }
    return result;
}

std::optional<PortMapping> parse_port_mapping(std::string_view port_mapping_string) {
    auto separator_idx = port_mapping_string.find(':');
    if (separator_idx == std::string_view::npos) {
        return {};
    }

    auto from_port = parse_port_num(port_mapping_string.substr(0, separator_idx));
    auto to_port = parse_port_num(port_mapping_string.substr(separator_idx + 1));

    if (!to_port || !from_port) {
        return {};
    }

    return PortMapping{ *from_port, *to_port };
}
