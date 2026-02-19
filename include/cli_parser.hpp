#pragma once
#include "types.hpp"
#include <expected>
#include <string>

namespace cli {

// Parses command-line arguments and returns populated CLI options.
// Returns an error string (or "help") via std::unexpected on invalid input.
std::expected<CliOptions, std::string> parse(int argc, char* argv[]);

// Prints usage and available options to standard output.
void print_help();

} // namespace cli