#pragma once

#include <string>
#include <string_view>

enum class TunnelErrc {
    // MAYBE ONE FOR BAD FORMAT ON IP invalid_
    empty_mapping,
    invalid_address,
};

struct Error {
    TunnelErrc errc;
    std::string context;
    Error with(std::string_view context) &&;
    Error with(std::string_view context) const&;
};
