#pragma once

/* Socket Monitor
 *
 * Ideally the logic for the tunnel needs to use non-blocking operations on
 * networks calls, otherwise I believe it would require a thread per connection
 * which isn't great (although it likely wouldn't have been a problem at my
 * scale).
 *
 * As such, the Linux epoll interface seemed like a natural choice to monitor
 * several socket file descriptors for activity from a single thread.
 *
 * The Socket Monitor class aims to be an abstracted away headless thread that
 * wraps epoll listening and produces events into a queue that worker threads
 * can then handle themselves.
 */

#include <thread>
#include <utility>

#include <sys/epoll.h>

#include "common/locking_queue.h"
#include "common/error.h"

class SocketMonitor {
public:
    enum class Event : uint8_t {
        Read   = EPOLLIN,
        HangUp = EPOLLHUP,
        Error  = EPOLLERR,
    };

    /* Creates an instance of a socket monitor. Events that occur on the
     * file descriptors that have been subscribed to will be pushed to the
     * event_queue */
    SocketMonitor();
    ~SocketMonitor();

    SocketMonitor& operator=(const SocketMonitor& other) = delete;
    SocketMonitor(const SocketMonitor& other) = delete;

    SocketMonitor& operator=(SocketMonitor&& other) noexcept = default;
    SocketMonitor(SocketMonitor&& other) noexcept = default;

    [[nodiscard]] std::optional<Error> subscribe_reader(int socket_fd) const;
    [[nodiscard]] std::optional<Error> rearm_reader(int socket_fd) const;

    [[nodiscard]] std::optional<Error> subscribe_sender(int socket_fd) const;
    [[nodiscard]] std::optional<Error> rearm_sender(int socket_fd) const;

    void unsubscribe(int socket_fd) const;

    /* To prevent possible race conditions where multiple threads could get
     * an event on the same file descriptor the Socket Monitor uses
     * EPOLLONESHOT which prevents new events from being created without
     * "re-arming" an event. Therefore we need to explicitly declare when
     * an event has been handled */

    std::optional<std::pair<int, Event>> pull_event(const std::stop_token& stop_token);
private:
    static constexpr uint32_t EPOLL_FLAGS     = EPOLLET | EPOLLONESHOT;
    static constexpr uint32_t READER_EVENTS   = EPOLLIN | EPOLLERR | EPOLLHUP;
    static constexpr uint32_t SENDER_EVENTS   = EPOLLERR | EPOLLHUP;

    static Event classify_event(uint32_t events);

    void monitor_job(const std::stop_token& stop_token);

    Queue<std::pair<int, Event>> event_queue_m;
    int epoll_fd_m;
    std::jthread listener_thread_m;
};
