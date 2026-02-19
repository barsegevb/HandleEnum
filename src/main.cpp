#include "cli_parser.hpp"
#include <iostream> // Fallback output stream for normal/error messages.
#include <format>   // Modern string formatting.
#include <cstdlib>

int main(int argc, char* argv[]) {
    // Parse command-line options into a strongly-typed options object.
    auto result = cli::parse(argc, argv);

    // Handle parser errors and the explicit help flow.
    if (!result) {
        if (result.error().empty()) {
            cli::print_help();
            return EXIT_SUCCESS;
        }
        // Print parsing error details and return failure status.
        std::cerr << std::format("Error: {}\n", result.error());
        return EXIT_FAILURE;
    }

    // Access validated options after successful parsing.
    const auto& options = result.value();

    // Temporary output until handle enumeration is wired in.
    if (options.verbose) {
        std::cout << "Verbose mode is ON\n";
    }
    
    if (options.pid) {
        std::cout << std::format("Filtering by PID: {}\n", *options.pid);
    }

    // Final scaffold confirmation.
    std::cout << "CLI Parser initialized successfully. Ready for next phase!\n";

    return EXIT_SUCCESS;
}