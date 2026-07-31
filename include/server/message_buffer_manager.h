#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <span>
#include <unordered_set>
#include <vector>

class MessageBufferManager {
  public:
    using BufferId = int;

    MessageBufferManager();
    ~MessageBufferManager();

    MessageBufferManager(const MessageBufferManager &) = delete;
    MessageBufferManager &operator=(const MessageBufferManager &) = delete;

    MessageBufferManager(MessageBufferManager &&) = delete;
    MessageBufferManager &operator=(MessageBufferManager &&) = delete;

    static constexpr std::size_t MESSAGE_BUFFER_SIZE = 1024;

    std::pair<BufferId, std::span<std::byte>> acquire_buffer();
    void release_buffer(BufferId buffer_id);

  private:
    using MessageBuffer = std::array<std::byte, MESSAGE_BUFFER_SIZE>;
    static constexpr std::size_t NUM_BUFFERS =
        50; // TODO this will need to change most likely

    std::vector<MessageBuffer> buffers_m;
    std::unordered_set<BufferId> available_buffers_m;

    mutable std::mutex mut_m;
    std::condition_variable cond_m;
};
