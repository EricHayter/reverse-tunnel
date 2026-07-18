#include "server/socket_monitor.h"
#include <cassert>
#include "spdlog/spdlog.h"

SocketMonitor::SocketMonitor()
    : epoll_fd_m(epoll_create1(0x00))
{
    if (epoll_fd_m == -1) {
        throw std::runtime_error("Failed to initialize epoll instance in socket monitor");
    }
    spdlog::debug("Initialized epoll instance with fd {}", int(epoll_fd_m));
    listener_thread_m = std::jthread(&SocketMonitor::monitor_job, this);
}


epoll_event SocketMonitor::create_listener_event(int socket_fd)
{
    epoll_event event{};
    event.events =  LISTENER_EVENTS | static_cast<uint32_t>(EPOLL_FLAGS);
    event.data.fd = socket_fd;
    return event;
}


std::optional<Error> SocketMonitor::subscribe(epoll_event event) const
{
    assert((event.events & EPOLLET) && (event.events & EPOLLONESHOT));
    int err = epoll_ctl(epoll_fd_m, EPOLL_CTL_ADD, event.data.fd, &event);
    if (err == -1) {
        switch (errno) {
        case EEXIST:
            return Error{ .context = "The given fd has already been subscribed to" };
        default:
            return Error{ .context = "Failed to subscribe to event" };
        }
    }
    return std::nullopt;
}

void SocketMonitor::unsubscribe(epoll_event event) const
{
    int err = epoll_ctl(epoll_fd_m, EPOLL_CTL_DEL, event.data.fd, nullptr);
    assert(err != -1);
}

std::optional<Error> SocketMonitor::rearm(epoll_event event) const
{
    event.events |= EPOLLET | EPOLLONESHOT;
    int err = epoll_ctl(epoll_fd_m, EPOLL_CTL_MOD, event.data.fd, &event);
    if (err == -1) {
        return Error{ .context = "Failed to rearm event" };
    }
    return std::nullopt;
}

std::optional<epoll_event> SocketMonitor::pull_event(const std::stop_token& stop_token)
{
    std::stop_callback callback(stop_token, [](){
        spdlog::debug("Stop requested. Preempting block.");
    });
    return event_queue_m.pop(stop_token);
}

SocketMonitor::~SocketMonitor()
{
    listener_thread_m.request_stop();
    close(epoll_fd_m);
}

void SocketMonitor::monitor_job(const std::stop_token& stop_token)
{
    // introduce a number of threads that we run at a time as a constant
    constexpr int TIMEOUT_MS = 1000;
    constexpr int MAX_EVENTS = 4;
    std::array<epoll_event, MAX_EVENTS> events{};
    while (!stop_token.stop_requested()) {
        int num_events = epoll_wait(epoll_fd_m, events.data(), MAX_EVENTS, TIMEOUT_MS);
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            // TODO find a way to handle this nicely? Need some communication
            // between this thread and everything else
            // something has gone horribly wrong
            return;
        }
        for (int i{ }; i < std::min(num_events, MAX_EVENTS); ++i) {
            event_queue_m.push(events[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
    }
}
