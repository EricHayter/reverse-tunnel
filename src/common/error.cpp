#include "common/error.h"

Error Error::with(std::string_view extra_context) && {
    context.insert(0, std::string(extra_context) + ": ");
    return std::move(*this);
}

Error Error::with(std::string_view extra_context) const& {
    return Error{*this}.with(extra_context);
}
