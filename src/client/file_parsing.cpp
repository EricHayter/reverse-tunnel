#include "client/file_parsing.h"
#include "common/parsing.h"

#include <fstream>
#include <stdexcept>
#include <string>

std::vector<PortMapping> parse_mapping_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);
    if (!file)
        throw std::runtime_error("cannot open " + file_path.string());

    std::vector<PortMapping> mappings;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;
        auto mapping = parse_port_mapping(line);
        if (!mapping)
            throw std::runtime_error("invalid mapping: " + line);
        mappings.push_back(*mapping);
    }
    return mappings;
}
