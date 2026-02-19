#include "printer.hpp"

#include <format>
#include <iostream>

void HandlePrinter::print_count_only(const CliOptions& options,
                                     const std::size_t total_raw_count,
                                     const std::size_t matching_count) const {
    if (options.verbose) {
        std::cout << "Verbose mode is ON\n";
    }

    if (options.pid) {
        std::cout << std::format("Filtering by PID: {}\n", *options.pid);
    }

    std::cout << std::format("Retrieved {} system handles.\n", total_raw_count);
    std::cout << std::format("Matching handles: {}\n", matching_count);
}

void HandlePrinter::print_results(const std::vector<HandleInfo>& handles,
                                  const CliOptions& options,
                                  const std::size_t total_raw_count) const {
    if (options.verbose) {
        std::cout << "Verbose mode is ON\n";
    }

    if (options.pid) {
        std::cout << std::format("Filtering by PID: {}\n", *options.pid);
    }

    std::cout << std::format("Retrieved {} system handles.\n", total_raw_count);

    std::cout << std::format("{:<8} {:<15} {:<10} {:<24} {}\n", "PID", "Process", "Handle", "Type", "Name");
    for (const HandleInfo& handle : handles) {
        std::cout << std::format("{:<8} {:<15} 0x{:<8X} {:<24} {}\n",
                                 handle.pid,
                                 handle.processName,
                                 handle.handleValue,
                                 handle.handleType,
                                 handle.objectName);
    }

    std::cout << std::format("Matching handles: {}\n", handles.size());
}

void HandlePrinter::print_header() const {
    std::cout << std::format("{:<8} {:<15} {:<10} {:<24} {}\n", "PID", "Process", "Handle", "Type", "Name");
}

void HandlePrinter::print_row(const HandleInfo& handle) const {
    std::cout << std::format("{:<8} {:<15} 0x{:<8X} {:<24} {}\n",
                             handle.pid,
                             handle.processName,
                             handle.handleValue,
                             handle.handleType,
                             handle.objectName);
}
