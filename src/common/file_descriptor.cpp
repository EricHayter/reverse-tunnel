#include "common/file_descriptor.h"

#include "spdlog/spdlog.h"
#include <cassert>
#include <unistd.h>

FileDescriptor::FileDescriptor(int file_descriptor)
    : owning_m{ true }
    , file_descriptor_m{ file_descriptor }
{
}

FileDescriptor::~FileDescriptor()
{
    if (owning_m) {
        spdlog::debug("Closing fd {}", file_descriptor_m);
        close(file_descriptor_m);
    }
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept
{
    if (this != &other) {
        if (owning_m) {
            close(this->file_descriptor_m);
        }
        owning_m = other.owning_m;
        other.owning_m = false;
        file_descriptor_m = other.file_descriptor_m;
    }
    return *this;
}

FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
    : owning_m{ other.owning_m }
    , file_descriptor_m{ other.file_descriptor_m }
{
    other.owning_m = false;
}

FileDescriptor::operator int() const
{
    assert(owning_m);
    return file_descriptor_m;
}


