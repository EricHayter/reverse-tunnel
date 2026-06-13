#include <iostream>
#include "client/args.h"

int main(int argc, const char** argv) {
    ClientConfig config;
    try {
        config = parse_mapping_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }

    if (config.mappings.empty()) {
        std::cerr << "No mappings provided. Aborting tunnel setup.\n";
        return 1;
    }

    for (const auto& m : config.mappings)
        std::cout << m.to << " -> " << m.from << '\n';

    // start client
    return 0;
}
