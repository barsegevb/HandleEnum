#include "app.hpp"

#include "cli_parser.hpp"
#include "nt.hpp"

#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <vector>

void HandleEnumApp::build_filters(const Parser& parsed_args) {
    m_filters.clear();

    if (parsed_args.pid.has_value()) {
        m_filters.push_back(std::make_unique<PidFilter>(*parsed_args.pid));
    }
}

int HandleEnumApp::run(int argc, char* argv[]) {
    auto parse_result = cli::parse(argc, argv);
    if (!parse_result) {
        if (parse_result.error() == "help") {
            cli::print_help();
            return EXIT_SUCCESS;
        }

        std::cerr << std::format("Error: {}\n", parse_result.error());
        return EXIT_FAILURE;
    }

    const Parser& options = parse_result.value();
    build_filters(options);

    if (auto privilege_result = nt::enable_debug_privilege(); !privilege_result) {
        std::cerr << std::format("Warning: failed to enable SeDebugPrivilege ({})\n",
                                 privilege_result.error().message());
    }

    auto handles_result = nt::query_system_handles();
    if (!handles_result) {
        std::cerr << std::format("Error: failed to query system handles ({})\n",
                                 handles_result.error().message());
        return EXIT_FAILURE;
    }

    std::vector<nt::RawHandle> filtered_handles;
    filtered_handles.reserve(handles_result->size());

    for (const nt::RawHandle& handle : *handles_result) {
        bool keep = true;
        for (const auto& filter : m_filters) {
            if (!filter->match(handle)) {
                keep = false;
                break;
            }
        }

        if (keep) {
            filtered_handles.push_back(handle);
        }
    }

    if (options.verbose) {
        std::cout << "Verbose mode is ON\n";
    }

    if (options.pid) {
        std::cout << std::format("Filtering by PID: {}\n", *options.pid);
    }

    std::cout << std::format("Retrieved {} system handles.\n", handles_result->size());

    if (options.showCountOnly) {
        std::cout << std::format("Matching handles: {}\n", filtered_handles.size());
    }

    return EXIT_SUCCESS;
}
