#include <iostream>
#include "client/args.h"
#include "client/connection.h"

int main(int argc, const char** argv) {
    auto config = parse_mapping_configuration(argc, argv);
    if (!config) {
        std::cerr << "error: " << config.error().context << '\n';
        return 1;
    }

    auto err = connect(config->ip_addr, config->mappings);
    if (err) {
        std::cerr << "error: " << err->context << '\n';
        return 1;
    }

    return 0;
}
