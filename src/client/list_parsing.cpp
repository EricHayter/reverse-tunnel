#include "client/list_parsing.h"
#include "common/parsing.h"

#include <string>

std::expected<std::vector<PortMapping>, Error> parse_mapping_list(std::string_view list) {
    std::vector<PortMapping> mappings;
    while (!list.empty()) {
        auto comma = list.find(',');
        auto segment = list.substr(0, comma);
        auto mapping = parse_port_mapping(segment);
        if (!mapping)
            return std::unexpected(Error{TunnelErrc::invalid_mapping,
                                         "invalid mapping: " + std::string(segment)});
        mappings.push_back(*mapping);
        if (comma == std::string_view::npos)
            break;
        list.remove_prefix(comma + 1);
    }
    return mappings;
}
