#include "client/args.h"

#include <fstream>
#include <string>

#include "spdlog/spdlog.h"
#include <argparse/argparse.hpp>

#include "common/parsing.h"

std::expected<std::vector<PortMapping>, Error> parse_mapping_list(std::string_view list) {
    std::vector<PortMapping> mappings;
    while (!list.empty()) {
        auto comma = list.find(',');
        auto segment = list.substr(0, comma);
        auto mapping = parse_port_mapping(segment);
        if (!mapping) {
            return std::unexpected(Error{.errc = TunnelErrc::invalid_mapping,
                                         .context = "invalid mapping: " + std::string(segment)});
        }
        mappings.push_back(*mapping);
        if (comma == std::string_view::npos) {
            break;
        }
        list.remove_prefix(comma + 1);
    }
    return mappings;
}

std::expected<std::vector<PortMapping>, Error>
parse_mapping_file(const std::filesystem::path &file_path) {
    std::ifstream file(file_path);
    if (!file) {
        return std::unexpected(Error{.errc = TunnelErrc::file_open_failed,
                                     .context = "cannot open " + file_path.string()});
    }
    std::vector<PortMapping> mappings;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        auto mapping = parse_port_mapping(line);
        if (!mapping) {
            return std::unexpected(
                Error{.errc = TunnelErrc::invalid_mapping, .context = "invalid mapping: " + line});
        }
        mappings.push_back(*mapping);
    }
    return mappings;
}

std::expected<ClientConfig, Error> parse_mapping_configuration(int argc, const char **argv) {
    argparse::ArgumentParser program("client");
    program.add_argument("addr").help("IP (v4 or v6) address of tunnel server");
    program.add_argument("-f", "--file")
        .help("file with one port mapping per line, each formatted as "
              "<from>:<to> "
              "(e.g. 80:8080)");
    program.add_argument("-l", "--list")
        .help("comma-separated list of port mappings, each formatted as "
              "<from>:<to> "
              "(e.g. 80:8080,22:2222)");
    program.add_argument("-v", "--verbose").help("increase output verbosity").flag();

    program.add_epilog("Port mapping format:\n"
                       "  Each mapping is <from>:<to>, where <from> and <to> "
                       "are port numbers.\n"
                       "\n"
                       "Examples:\n"
                       "  client 192.168.1.5 -l 80:8080\n"
                       "  client 192.168.1.5 -l 80:8080,22:2222\n"
                       "  client 192.168.1.5 -f mappings.txt");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &err) {
        return std::unexpected(Error{.errc = TunnelErrc::invalid_arguments, .context = err.what()});
    }

    ClientConfig config;

    config.ip_addr = program.get("addr");

    if (program["--verbose"] == true) {
        spdlog::set_level(spdlog::level::debug); // Set *global* log level to debug
    }

    if (auto file_path = program.present("-f")) {
        auto file_mappings = parse_mapping_file(*file_path);
        if (!file_mappings) {
            return std::unexpected(std::move(file_mappings).error().with("parsing --file"));
        }
        config.mappings.insert(config.mappings.end(), file_mappings->begin(), file_mappings->end());
    }

    if (auto list = program.present("-l")) {
        auto list_mappings = parse_mapping_list(*list);
        if (!list_mappings) {
            return std::unexpected(std::move(list_mappings).error().with("parsing --list"));
        }
        config.mappings.insert(config.mappings.end(), list_mappings->begin(), list_mappings->end());
    }

    if (config.mappings.empty()) {
        return std::unexpected(
            Error{.errc = TunnelErrc::empty_mapping, .context = "no port mappings provided"});
    }

    return config;
}
