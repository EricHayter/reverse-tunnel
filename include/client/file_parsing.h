#pragma once

#include <filesystem>
#include <vector>

struct PortMapping;

std::vector<PortMapping> parse_mapping_file(const std::filesystem::path& file_path);
