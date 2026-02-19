#include "app.hpp"

#include "cli_parser.hpp"
#include "nt.hpp"

#include <cstdlib>
#include <format>
#include <iostream>

int HandleEnumApp::run(int argc, char* argv[]) const {
    auto parse_result = cli::parse(argc, argv);
    if (!parse_result) {
        if (parse_result.error() == "help") {
            cli::print_help();
            return EXIT_SUCCESS;
        }

        std::cerr << std::format("Error: {}\n", parse_result.error());
        return EXIT_FAILURE;
    }

    const auto& options = parse_result.value();

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

    if (options.verbose) {
        std::cout << "Verbose mode is ON\n";
    }

    if (options.pid) {
        std::cout << std::format("Filtering by PID: {}\n", *options.pid);
    }

    std::cout << std::format("Retrieved {} system handles.\n", handles_result->size());

    return EXIT_SUCCESS;
}
