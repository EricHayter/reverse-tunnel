#include "client/list_parsing.h"
#include "common/parsing.h"

#include <stdexcept>
#include <string>

std::vector<PortMapping> parse_mapping_list(std::string_view list) {
    std::vector<PortMapping> mappings;
    while (!list.empty()) {
        auto comma = list.find(',');
        auto segment = list.substr(0, comma);
        auto mapping = parse_port_mapping(segment);
        if (!mapping)
            throw std::runtime_error("invalid mapping: " + std::string(segment));
        mappings.push_back(*mapping);
        if (comma == std::string_view::npos)
            break;
        list.remove_prefix(comma + 1);
    }
    return mappings;
}
