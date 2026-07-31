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

class SocketMonitor {
  public:
    /* An event reported by the monitor for a ready fd */
    enum class Event : uint8_t {
        Read = EPOLLIN,
        Write = EPOLLOUT,
        HangUp = EPOLLHUP,
        Error = EPOLLERR,
    };

    /* The interest that can be armed on an fd. Read interest lives on the read
     * epoll instance and write interest on the write instance, so a fd's read
     * and write oneshot lifecycles are fully independent */
    enum class Interest : uint8_t {
        Read = EPOLLIN,
        Write = EPOLLOUT,
    };

    /* Creates an instance of a socket monitor. Events that occur on the
     * file descriptors that have been subscribed to will be pushed to the
     * event_queue */
    SocketMonitor();
    ~SocketMonitor();

    SocketMonitor &operator=(const SocketMonitor &other) = delete;
    SocketMonitor(const SocketMonitor &other) = delete;

    SocketMonitor &operator=(SocketMonitor &&other) noexcept = default;
    SocketMonitor(SocketMonitor &&other) noexcept = default;

    /* Arms (or re-arms) the given interest on the fd. Adds the fd to the
     * relevant epoll instance if it is not registered yet, otherwise modifies
     * it, so this doubles as both the initial subscribe and the post-oneshot
     * re-arm.
     *
     * A failing epoll_ctl here only happens on unrecoverable resource
     * exhaustion (ENOSPC/ENOMEM), so it logs and aborts rather than reporting
     * an error the caller cannot act on */
    void arm(int socket_fd, Interest interest) const;

    /* Clears the given interest on the fd without unregistering it, so it can
     * be armed again later. Useful to cancel a still-armed interest that has
     * not fired yet. Aborts on failure for the same reason as arm */
    void disarm(int socket_fd, Interest interest) const;

    /* Removes the fd from both epoll instances entirely */
    void unsubscribe(int socket_fd) const;

    /* To prevent possible race conditions where multiple threads could get
     * an event on the same file descriptor the Socket Monitor uses
     * EPOLLONESHOT which prevents new events from being created without
     * "re-arming" an event. Therefore we need to explicitly declare when
     * an event has been handled */

    std::optional<std::pair<int, Event>>
    pull_event(const std::stop_token &stop_token);

  private:
    static constexpr uint32_t EPOLL_FLAGS = EPOLLET | EPOLLONESHOT;

    /* Turns the events reported for a ready fd into a single Event. The read
     * instance reports Read/HangUp/Error; the write instance only reports Write
     * (HangUp/Error are left to the read instance to avoid duplicates) */
    static std::optional<Event> classify_event(uint32_t events, bool is_reader);

    void monitor_job(const std::stop_token &stop_token, int epoll_fd,
                     bool is_reader);

    Queue<std::pair<int, Event>> event_queue_m;
    int epoll_read_fd_m;
    int epoll_write_fd_m;
    std::jthread read_listener_m;
    std::jthread write_listener_m;
};
