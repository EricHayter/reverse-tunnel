#include <iostream>
#include "client/args.h"

int main(int argc, const char** argv) {
    auto config = parse_mapping_args(argc, argv);
    if (!config) {
        std::cerr << "error: " << config.error().context << '\n';
        return 1;
    }

    for (const auto& m : config->mappings)
        std::cout << m.to << " -> " << m.from << '\n';

    // start client
    return 0;
}
