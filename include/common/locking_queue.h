#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T> class Queue {
  public:
    Queue() = default;
    ~Queue();

    Queue &operator=(const Queue &) = delete;
    Queue(const Queue &) = delete;

    Queue &operator=(Queue &&) noexcept = default;
    Queue(Queue &&) noexcept = default;

    void push(T val);

    /* Blocks until an element is ready to be popped from the queue. In the
     * case that the worker thread must be stopped (i.e. stop_requested) we
     * take a stop_token such that the blocking will be interrupted and early
     * return a std::nullopt */
    std::optional<T> pop(const std::stop_token &stop_token);

  private:
    std::unique_ptr<std::mutex> mut_m{std::make_unique<std::mutex>()};
    std::unique_ptr<std::condition_variable_any> cond_m{
        std::make_unique<std::condition_variable_any>()};
    std::queue<T> queue_m;
};

template <typename T> Queue<T>::~Queue() { cond_m->notify_all(); }

template <typename T> void Queue<T>::push(T val) {
    const std::lock_guard<std::mutex> lock{*mut_m};
    queue_m.push(std::move(val));
    cond_m->notify_one();
}

template <typename T> std::optional<T> Queue<T>::pop(const std::stop_token &stop_token) {
    std::unique_lock<std::mutex> lock{*mut_m};
    cond_m->wait(lock, stop_token, [this]() { return !queue_m.empty(); });
    if (stop_token.stop_requested()) {
        return std::nullopt;
    }
    T ret = std::move(queue_m.front());
    queue_m.pop();
    return ret;
}
