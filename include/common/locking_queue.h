#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class Queue {
public:
    Queue() = default;
    ~Queue();

    Queue& operator=(const Queue&) = delete;
    Queue(const Queue&) = delete;

    void push(T val);

    /* Blocks until an element is ready to be popped from the queue. In the
     * case that the worker thread must be stopped (i.e. stop_requested) we
     * take a stop_token such that the blocking will be interrupted and early
     * return a std::nullopt */
    std::optional<T> pop(std::stop_token st);
private:
    std::mutex mut_m;
    std::condition_variable_any cond_m;
    std::queue<T> queue_m;
};

template<typename T>
Queue<T>::~Queue()
{
    cond_m.notify_all();
}

template<typename T>
void Queue<T>::push(T val)
{
    std::lock_guard<std::mutex> lg{ mut_m };
    queue_m.push(std::move(val));
    cond_m.notify_one();
}


template<typename T>
std::optional<T> Queue<T>::pop(std::stop_token st)
{
    std::unique_lock<std::mutex> lk{ mut_m };
    cond_m.wait(lk, st, [this](){ return !queue_m.empty(); });
    if (st.stop_requested())
        return std::nullopt;
    T ret = std::move(queue_m.front());
    queue_m.pop();
    return ret;
}
