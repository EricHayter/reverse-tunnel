#pragma once

#include <expected>

#include "client/definitions.h"
#include "common/error.h"

std::expected<ClientConfig, Error> parse_mapping_args(int argc, const char** argv);
