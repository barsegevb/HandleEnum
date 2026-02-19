#pragma once
#include <string>
#include <optional>
#include <vector>
#include <cstdint>

// Defines the supported sort keys for output ordering.
enum class SortField { Pid, Type, Name };

// Holds all parsed command-line filters and mode switches.
struct CliOptions {
    // Optional process ID filter.
    std::optional<uint32_t> pid;
    // Optional process-name filter (string match performed later).
    std::optional<std::string> processName;
    // Optional handle-type filter.
    std::optional<std::string> handleType;
    // Optional object-name filter.
    std::optional<std::string> objectName;
    // Output sorting strategy.
    SortField sortBy = SortField::Pid;
    // If true, print aggregate counts only.
    bool showCountOnly = false;
    // If true, print additional diagnostics/details.
    bool verbose = false;
};