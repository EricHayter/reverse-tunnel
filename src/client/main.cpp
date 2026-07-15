#include <iostream>
#include "client/args.h"
#include "client/client.h"

int main(int argc, const char** argv) {
    auto config = parse_mapping_configuration(argc, argv);
    if (!config) {
        std::cerr << "error: " << config.error().context << '\n';
        return 1;
    }

    const Client client{ config->ip_addr, config->mappings };

    return 0;
}
