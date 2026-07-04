#pragma once

#include <expected>
#include <filesystem>
#include <string_view>
#include <vector>

#include "client/definitions.h"
#include "common/error.h"

// Parses a comma-separated list of port mappings, e.g. "22:2222,80:8080".
// Returns a TunnelErrc::invalid_mapping error if any mapping is invalid.
std::expected<std::vector<PortMapping>, Error> parse_mapping_list(std::string_view list);

std::expected<std::vector<PortMapping>, Error> parse_mapping_file(const std::filesystem::path& file_path);

std::expected<ClientConfig, Error> parse_mapping_configuration(int argc, const char** argv);
