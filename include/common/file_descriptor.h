#pragma once

/* RAII class for handling file descriptors to avoid leaks */
class FileDescriptor {
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int file_descriptor);
    ~FileDescriptor();

    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(const FileDescriptor&) = delete;

    FileDescriptor& operator=(FileDescriptor&& other) noexcept;
    FileDescriptor(FileDescriptor&& other) noexcept;

    operator int() const; // NOLINT(hicpp-explicit-conversions)

private:
    bool owning_m{ false };
    int file_descriptor_m{};
};
