#include "server/message_buffer_manager.h"

#include <cassert>

MessageBufferManager::MessageBufferManager()
    : buffers_m(NUM_BUFFERS)
{
    for (std::size_t i = 0; i < NUM_BUFFERS; i++) {
        available_buffers_m.insert(BufferId(i));
    }
}

std::pair<MessageBufferManager::BufferId, std::span<std::byte>> MessageBufferManager::acquire_buffer()
{
    std::unique_lock<std::mutex> lock(mut_m);
    cond_m.wait(lock, [this](){
        return !available_buffers_m.empty();
    });

    BufferId buffer_id = *std::begin(available_buffers_m);
    available_buffers_m.extract(buffer_id);
    return { buffer_id, buffers_m[buffer_id] };
}

void MessageBufferManager::release_buffer(BufferId buffer_id)
{
    std::lock_guard<std::mutex> lock(mut_m);
    assert(!available_buffers_m.contains(buffer_id));
    available_buffers_m.insert(buffer_id);
    cond_m.notify_one();
}
