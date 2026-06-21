#include "client/args.h"

#include <argparse/argparse.hpp>

#include "client/file_parsing.h"
#include "client/list_parsing.h"

std::expected<ClientConfig, Error> parse_mapping_args(int argc, const char** argv) {
    argparse::ArgumentParser program("client");
    program.add_argument("addr")
        .help("IP (v4 or v6) address of tunnel server");
    program.add_argument("-f", "--file")
        .help("file containing a list of client-server port mappings");
    program.add_argument("-l", "--list")
        .help("list of client-server port mappings");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        return std::unexpected(Error{TunnelErrc::invalid_arguments, err.what()});
    }

    ClientConfig config;

    config.ip_addr = program.get("addr");

    if (auto file_path = program.present("-f")) {
        auto file_mappings = parse_mapping_file(*file_path);
        if (!file_mappings)
            return std::unexpected(std::move(file_mappings).error().with("parsing --file"));
        config.mappings.insert(config.mappings.end(),
                               file_mappings->begin(), file_mappings->end());
    }

    if (auto list = program.present("-l")) {
        auto list_mappings = parse_mapping_list(*list);
        if (!list_mappings)
            return std::unexpected(std::move(list_mappings).error().with("parsing --list"));
        config.mappings.insert(config.mappings.end(),
                               list_mappings->begin(), list_mappings->end());
    }

    if (config.mappings.empty())
        return std::unexpected(Error{TunnelErrc::empty_mapping, "no port mappings provided"});

    return config;
}
