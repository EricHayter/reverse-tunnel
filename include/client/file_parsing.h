#pragma once

#include <expected>
#include <filesystem>
#include <vector>

#include "common/error.h"

struct PortMapping;

std::expected<std::vector<PortMapping>, Error> parse_mapping_file(const std::filesystem::path& file_path);
