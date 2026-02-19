#include "printer.hpp"

#include <format>
#include <iostream>

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

    if (options.showCountOnly) {
        std::cout << std::format("Matching handles: {}\n", handles.size());
        return;
    }

    std::cout << std::format("{:<8} {:<15} {:<24} {}\n", "PID", "Process", "Type", "Name");
    for (const HandleInfo& handle : handles) {
        std::cout << std::format("{:<8} {:<15} {:<24} {}\n",
                                 handle.pid,
                                 handle.processName,
                                 handle.handleType,
                                 handle.objectName);
    }

    std::cout << std::format("Matching handles: {}\n", handles.size());
}
