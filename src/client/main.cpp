#include <iostream>
#include <argparse/argparse.hpp>
#include "client/file_parsing.h"
#include "client/list_parsing.h"
#include "common/definitions.h"

int main(int argc, const char** argv) {
    argparse::ArgumentParser program("client");
    program.add_argument("-f", "--file")
        .help("file containing a list of client-server port mappings");
    program.add_argument("-l", "--list")
        .help("list of client-server port mappings");

    try {
      program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
      std::cerr << err.what() << std::endl;
      std::cerr << program;
      std::exit(1);
    }

    std::vector<PortMapping> mappings;

    if (auto file_path = program.present("-f")) {
        try {
            std::vector<PortMapping> file_mappings = parse_mapping_file(*file_path);
            for (auto mapping: file_mappings) {
                mappings.push_back(mapping);
            }
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
    }

    if (auto list = program.present("-l")) {
        try {
            auto list_mappings = parse_mapping_list(*list);
            for (auto mapping : list_mappings)
                mappings.push_back(mapping);
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
    }

    if (mappings.empty()) {
        std::cerr << "no mappings provided\n";
        return 1;
    }

    for (const auto& m : mappings)
        std::cout << m.to << " -> " << m.from << '\n';

    // start client
}
