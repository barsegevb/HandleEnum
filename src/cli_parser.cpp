#include "cli_parser.hpp"
#include <expected>
#include <string_view>
#include <iostream>
#include <format>
#include <vector>
#include <map>
#include <functional>

namespace cli {

/**
 * @brief Parses command-line arguments using a command-mapping approach.
 * This eliminates long if-else chains and makes the code highly extensible.
 */
std::expected<CliOptions, std::string> parse(int argc, char* argv[]) {
    CliOptions options;
    std::vector<std::string_view> args(argv + 1, argv + argc);

    // Type definition for our argument handlers
    using Handler = std::function<std::expected<void, std::string>(size_t& i)>;

    // Mapping flags to their respective logic
    std::map<std::string_view, Handler> handlers = {
        {"-p", [&](size_t& i) -> std::expected<void, std::string> {
            if (++i >= args.size()) return std::unexpected("Missing value for -p");
            try { options.pid = std::stoul(std::string(args[i])); return {}; }
            catch (...) { return std::unexpected(std::format("Invalid PID: {}", args[i])); }
        }},

        {"-n", [&](size_t& i) -> std::expected<void, std::string> {
            if (++i >= args.size()) return std::unexpected("Missing value for -n");
            options.processName = std::string(args[i]); return {};
        }},

        {"-t", [&](size_t& i) -> std::expected<void, std::string> {
            if (++i >= args.size()) return std::unexpected("Missing value for -t");
            options.handleType = std::string(args[i]); return {};
        }},

        {"-o", [&](size_t& i) -> std::expected<void, std::string> {
            if (++i >= args.size()) return std::unexpected("Missing value for -o");
            options.objectName = std::string(args[i]); return {};
        }},

        {"-s", [&](size_t& i) -> std::expected<void, std::string> {
            if (++i >= args.size()) return std::unexpected("Missing value for -s");
            if (args[i] == "pid") options.sortBy = SortField::Pid;
            else if (args[i] == "type") options.sortBy = SortField::Type;
            else if (args[i] == "name") options.sortBy = SortField::Name;
            else return std::unexpected(std::format("Invalid sort field: {}", args[i]));
            return {};
        }},

        {"-c", [&](size_t&) -> std::expected<void, std::string> { options.showCountOnly = true; return {}; }},

        {"-v", [&](size_t&) -> std::expected<void, std::string> { options.verbose = true; return {}; }},

        {"-h", [&](size_t&) -> std::expected<void, std::string> { return std::unexpected("help"); }}
    };

    handlers["--pid"] = handlers.at("-p");
    handlers["--name"] = handlers.at("-n");
    handlers["--type"] = handlers.at("-t");
    handlers["--object"] = handlers.at("-o");
    handlers["--sort"] = handlers.at("-s");
    handlers["--count"] = handlers.at("-c");
    handlers["--verbose"] = handlers.at("-v");
    handlers["--help"] = handlers.at("-h");

    // Main parsing loop
    for (size_t i = 0; i < args.size(); ++i) {
        if (auto it = handlers.find(args[i]); it != handlers.end()) {
            auto result = it->second(i);
            if (!result) return std::unexpected(result.error());
        } else {
            return std::unexpected(std::format("Unknown argument: {}", args[i]));
        }
    }

    return options;
}

void print_help() {
    std::cout << "HandleEnum.exe [OPTIONS]\n\n"
              << "Options:\n"
              << "  -p, --pid <PID>          Filter by process ID\n"
              << "  -n, --name <ProcessName> Filter by process name\n"
              << "  -t, --type <HandleType>  Filter by handle type\n"
              << "  -o, --object <ObjectName> Filter by object name (substring)\n"
              << "  -s, --sort <Field>       Sort by: pid, type, name (default: pid)\n"
              << "  -c, --count              Show only count statistics\n"
              << "  -v, --verbose            Show detailed info\n"
              << "  -h, --help               Display help message\n";
}

} // namespace cli