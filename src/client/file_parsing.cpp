#include "client/file_parsing.h"
#include "common/parsing.h"

#include <fstream>
#include <string>

std::expected<std::vector<PortMapping>, Error> parse_mapping_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file)
        return std::unexpected(Error{TunnelErrc::file_open_failed,
                                     "cannot open " + file_path.string()});

    std::vector<PortMapping> mappings;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto mapping = parse_port_mapping(line);
        if (!mapping)
            return std::unexpected(Error{TunnelErrc::invalid_mapping,
                                         "invalid mapping: " + line});
        mappings.push_back(*mapping);
    }
    return mappings;
}
