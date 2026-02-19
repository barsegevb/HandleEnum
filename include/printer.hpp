#pragma once

#include "types.hpp"

#include <cstddef>
#include <vector>

class HandlePrinter {
public:
    void print_results(const std::vector<HandleInfo>& handles,
                       const CliOptions& options,
                       std::size_t total_raw_count) const;
};
