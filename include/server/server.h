#pragma once

#include <vector>
#include <thread>
#include <condition_variable>
#include <unordered_map>

#include "common/error.h"
#include "common/locking_queue.h"

class Server {
public:
    Server(std::size_t num_workers);
    Error handle_handshake();

private:
    std::vector<int> fds_m; // TODO might not need this since EPOLL tracks it?

    // EPOLL thread
    void coordinator_func(std::stop_token st);
    std::jthread event_thread_m;
    void wait_events();

    // worker pool stuff
    std::vector<std::jthread> workers_m;
    void worker_func(std::stop_token st);
    Queue<int> queue_m;
    std::condition_variable cond_m;
    std::mutex mut_m;

    // stores from-to mapping
    std::unordered_map<int, int> port_map_m;
};

// does this even work for single threaded CPUs? Problem: wasting too much
// time on the EPOLL thread without pre-emption
// SOLUTION: I think I can use std::yield? Might need to test if this is
// actually a valid solution
// then use hardware_concurrency
