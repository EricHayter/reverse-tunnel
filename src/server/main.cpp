#include <iostream>

#include <argparse/argparse.hpp>
#include "spdlog/spdlog.h"

#include "server/server.h"


int main(int argc, const char** argv) {
    argparse::ArgumentParser program("server");
    program.add_argument("-v", "--verbose")
        .help("increase output verbosity")
        .flag();

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    if (program["--verbose"] == true) {
        spdlog::set_level(spdlog::level::debug); // Set *global* log level to debug
    }

    spdlog::info("Starting server!\n");
    Server server{ 4 };
    auto err = server.handle_handshake();
}
