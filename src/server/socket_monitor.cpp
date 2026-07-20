#include "server/socket_monitor.h"
#include <cassert>
#include "spdlog/spdlog.h"

SocketMonitor::SocketMonitor()
    : epoll_fd_m(epoll_create1(0x00))
{
    if (epoll_fd_m == -1) {
        throw std::runtime_error("Failed to initialize epoll instance in socket monitor");
    }
    spdlog::debug("Initialized epoll instance with fd {}", epoll_fd_m);
    listener_thread_m = std::jthread(&SocketMonitor::monitor_job, this);
}


static std::optional<Error> epoll_add(int epoll_fd, int socket_fd, uint32_t event_mask)
{
    epoll_event event{};
    event.events = event_mask;
    event.data.fd = socket_fd;

    int err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
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

static std::optional<Error> epoll_mod(int epoll_fd, int socket_fd, uint32_t event_mask)
{
    epoll_event event{};
    event.events = event_mask;
    event.data.fd = socket_fd;

    int err = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_fd, &event);
    if (err == -1) {
        return Error{ .context = "Failed to rearm event" };
    }
    return std::nullopt;
}

std::optional<Error> SocketMonitor::subscribe_reader(int socket_fd) const
{
    return epoll_add(epoll_fd_m, socket_fd, READER_EVENTS | EPOLL_FLAGS);
}

std::optional<Error> SocketMonitor::rearm_reader(int socket_fd) const
{
    return epoll_mod(epoll_fd_m, socket_fd, READER_EVENTS | EPOLL_FLAGS);
}

std::optional<Error> SocketMonitor::subscribe_sender(int socket_fd) const
{
    return epoll_add(epoll_fd_m, socket_fd, SENDER_EVENTS | EPOLL_FLAGS);
}

std::optional<Error> SocketMonitor::rearm_sender(int socket_fd) const
{
    return epoll_mod(epoll_fd_m, socket_fd, SENDER_EVENTS | EPOLL_FLAGS);
}

void SocketMonitor::unsubscribe(int socket_fd) const
{
    int err = epoll_ctl(epoll_fd_m, EPOLL_CTL_DEL, socket_fd, nullptr);
    assert(err != -1);
}

SocketMonitor::Event SocketMonitor::classify_event(uint32_t events)
{
    if ((events & EPOLLHUP) != 0U) { return SocketMonitor::Event::HangUp; }
    if ((events & EPOLLERR) != 0U) { return SocketMonitor::Event::Error; }
    return SocketMonitor::Event::Read;
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
    listener_thread_m.request_stop();
    close(epoll_fd_m);
}

void SocketMonitor::monitor_job(const std::stop_token& stop_token)
{
    constexpr int TIMEOUT_MS = 1000;
    constexpr int MAX_EVENTS = 4;
    std::array<epoll_event, MAX_EVENTS> events{};
    while (!stop_token.stop_requested()) {
        int num_events = epoll_wait(epoll_fd_m, events.data(), MAX_EVENTS, TIMEOUT_MS);
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        for (int i{ }; i < std::min(num_events, MAX_EVENTS); ++i) {
            const epoll_event& epoll_ev = events[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            event_queue_m.push({ epoll_ev.data.fd, classify_event(epoll_ev.events) });
        }
    }
}
