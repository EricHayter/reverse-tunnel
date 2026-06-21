#pragma once

#include <string_view>
#include <span>
#include <optional>

#include "common/definitions.h"
#include "common/error.h"

std::optional<Error> connect(std::string_view addr_string, std::span<PortMapping> mappings);
