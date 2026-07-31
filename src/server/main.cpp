#include <csignal>
#include <exception>
#include <iostream>

#include "spdlog/spdlog.h"
#include <argparse/argparse.hpp>

#include "server/server.h"

int main(int argc, const char **argv) {
    argparse::ArgumentParser program("server");
    program.add_argument("-v", "--verbose")
        .help("increase output verbosity")
        .flag();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &err) {
        std::cerr << err.what() << '\n';
        std::cerr << program;
        return 1;
    }

    if (program["--verbose"] == true) {
        spdlog::set_level(
            spdlog::level::debug); // Set *global* log level to debug
    }

    // Block termination signals before spawning any worker threads. Threads
    // inherit this mask, so the signal is delivered synchronously to sigwait()
    // below rather than interrupting an arbitrary worker.
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGINT);
    sigaddset(&signal_mask, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &signal_mask, nullptr) != 0) {
        spdlog::error("Failed to install signal mask");
        return 1;
    }

    try {
        spdlog::info("Starting server");
        Server server{4};

        // Sleep here until the user asks us to stop (Ctrl-C / SIGTERM). No busy
        // loop: sigwait blocks the thread and uses no CPU.
        int received_signal{0};
        sigwait(&signal_mask, &received_signal);
        spdlog::info("Received signal {}, shutting down", received_signal);
    } catch (const std::exception &err) {
        spdlog::error("Server terminated: {}", err.what());
        return 1;
    }
    // server's destructor runs here, stopping and joining the worker/monitor
    // threads before we exit.

    return 0;
}
