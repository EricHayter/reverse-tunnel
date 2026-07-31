#include "server/socket_monitor.h"
#include <cassert>
#include "spdlog/spdlog.h"

SocketMonitor::SocketMonitor()
    : epoll_read_fd_m(epoll_create1(0x00)),
      epoll_write_fd_m(epoll_create1(0x00))
{
    if (epoll_read_fd_m == -1 || epoll_write_fd_m == -1) {
        throw std::runtime_error("Failed to initialize epoll instance in socket monitor");
    }
    spdlog::debug("Initialized epoll instances with fds {} (read) and {} (write)",
            epoll_read_fd_m, epoll_write_fd_m);
    read_listener_m  = std::jthread(&SocketMonitor::monitor_job, this, epoll_read_fd_m, true);
    write_listener_m = std::jthread(&SocketMonitor::monitor_job, this, epoll_write_fd_m, false);
}


std::optional<Error> SocketMonitor::arm(int socket_fd, Interest interest) const
{
    int epoll_fd = (interest == Interest::Read) ? epoll_read_fd_m : epoll_write_fd_m;

    epoll_event event{};
    event.events = static_cast<uint32_t>(interest) | EPOLL_FLAGS;
    event.data.fd = socket_fd;

    /* Modify if the fd is already registered on this instance, otherwise add
     * it. This lets arm serve as both the first subscribe and the re-arm after
     * a oneshot event without the caller tracking which it is */
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_fd, &event) == 0) {
        return std::nullopt;
    }
    if (errno == ENOENT && epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == 0) {
        return std::nullopt;
    }
    return Error{ .context = "Failed to arm interest on fd" };
}

std::optional<Error> SocketMonitor::disarm(int socket_fd, Interest interest) const
{
    int epoll_fd = (interest == Interest::Read) ? epoll_read_fd_m : epoll_write_fd_m;

    /* Keep the fd registered with an empty interest mask so it can be armed
     * again later (an empty mask still delivers EPOLLHUP/EPOLLERR) */
    epoll_event event{};
    event.events = 0;
    event.data.fd = socket_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_fd, &event) == -1) {
        return Error{ .context = "Failed to disarm interest on fd" };
    }
    return std::nullopt;
}

void SocketMonitor::unsubscribe(int socket_fd) const
{
    /* The fd may only be registered on one instance, so a missing registration
     * (ENOENT) is not treated as an error here */
    epoll_ctl(epoll_read_fd_m, EPOLL_CTL_DEL, socket_fd, nullptr);
    epoll_ctl(epoll_write_fd_m, EPOLL_CTL_DEL, socket_fd, nullptr);
}

std::optional<SocketMonitor::Event> SocketMonitor::classify_event(uint32_t events, bool is_reader)
{
    /* The write instance only reports writability; hangup and error are left to
     * the read instance so a fd registered on both does not surface them twice */
    if (!is_reader) {
        if ((events & EPOLLOUT) != 0U) { return SocketMonitor::Event::Write; }
        return std::nullopt;
    }

    if ((events & EPOLLHUP) != 0U) { return SocketMonitor::Event::HangUp; }
    if ((events & EPOLLERR) != 0U) { return SocketMonitor::Event::Error; }
    if ((events & EPOLLIN)  != 0U) { return SocketMonitor::Event::Read; }
    return std::nullopt;
}

std::optional<std::pair<int, SocketMonitor::Event>> SocketMonitor::pull_event(const std::stop_token& stop_token)
{
    std::stop_callback callback(stop_token, [](){
        spdlog::debug("Stop requested. Preempting block.");
    });
    return event_queue_m.pop(stop_token);
}

SocketMonitor::~SocketMonitor()
{
    read_listener_m.request_stop();
    write_listener_m.request_stop();
    close(epoll_read_fd_m);
    close(epoll_write_fd_m);
}

void SocketMonitor::monitor_job(const std::stop_token& stop_token, int epoll_fd, bool is_reader)
{
    constexpr int TIMEOUT_MS = 1000;
    constexpr int MAX_EVENTS = 4;
    std::array<epoll_event, MAX_EVENTS> events{};
    while (!stop_token.stop_requested()) {
        int num_events = epoll_wait(epoll_fd, events.data(), MAX_EVENTS, TIMEOUT_MS);
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        for (int i{ }; i < std::min(num_events, MAX_EVENTS); ++i) {
            const epoll_event& epoll_ev = events[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            if (auto event = classify_event(epoll_ev.events, is_reader)) {
                event_queue_m.push({ epoll_ev.data.fd, *event });
            }
        }
    }
}
