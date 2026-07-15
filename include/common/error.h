#pragma once

#include <string>
#include <string_view>
#include <cstdint>

enum class TunnelErrc : uint8_t {
    // MAYBE ONE FOR BAD FORMAT ON IP invalid_
    empty_mapping,
    invalid_address,
    invalid_mapping,
    file_open_failed,
    invalid_arguments,
    socket_creation_failure,
};

struct Error {
    TunnelErrc errc;
    std::string context;
    Error with(std::string_view context) &&;

    [[nodiscard]]
    Error with(std::string_view context) const&;
};
