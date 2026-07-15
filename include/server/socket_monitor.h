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

#include <sys/epoll.h>

#include "common/locking_queue.h"
#include "common/error.h"
#include "common/file_descriptor.h"

class SocketMonitor {
public:
    /* Creates an instance of a socket monitor. Events that occur on the
     * file descriptors that have been subscribed to will be pushed to the
     * event_queue */
    SocketMonitor();
    ~SocketMonitor();

    SocketMonitor& operator=(const SocketMonitor& other) = delete;
    SocketMonitor(const SocketMonitor& other) = delete;

    SocketMonitor& operator=(SocketMonitor&& other) noexcept = default;
    SocketMonitor(SocketMonitor&& other) noexcept = default;



    static constexpr int EPOLL_FLAGS  = EPOLLET | EPOLLONESHOT;

    // helper function for creating an epoll event for listeners
    static epoll_event create_listener_event(int socket_fd);
    static constexpr unsigned int LISTENER_EVENTS = EPOLLIN | EPOLLERR | EPOLLHUP;


    // TODO maybe implement moving stuff later

    /* Subscribe to a given file descriptor for events. Events on this file
     * descriptor will be pushed to event_queue */
    std::optional<Error> subscribe(epoll_event event);

    void unsubscribe(epoll_event event) const;

    /* To prevent possible race conditions where multiple threads could get
     * an event on the same file descriptor the Socket Monitor uses
     * EPOLLONESHOT which prevents new events from being created without
     * "re-arming" an event. Therefore we need to explicitly declare when
     * an event has been handled */
    std::optional<Error> rearm(epoll_event event);

    std::optional<epoll_event> pull_event(const std::stop_token& stop_token);
private:
    void monitor_job(const std::stop_token& stop_token);

    Queue<epoll_event> event_queue_m;
    FileDescriptor epoll_fd_m;
    std::jthread listener_thread_m;
};
