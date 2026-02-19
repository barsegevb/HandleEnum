#include "cli_parser.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

std::expected<CliOptions, std::string> parse_args(std::initializer_list<const char*> args) {
    std::vector<std::string> owned;
    owned.reserve(args.size() + 1);
    owned.emplace_back("HandleEnum.exe");
    for (const char* arg : args) {
        owned.emplace_back(arg);
    }

    std::vector<char*> argv;
    argv.reserve(owned.size());
    for (std::string& item : owned) {
        argv.push_back(const_cast<char*>(item.c_str()));
    }

    return cli::parse(static_cast<int>(argv.size()), argv.data());
}

void test_short_flags_success() {
    auto result = parse_args({"-p", "1234", "-n", "notepad.exe", "-t", "File", "-o", "kernel32", "-s", "type", "-c", "-v"});
    expect_true(result.has_value(), "short flags should parse successfully");
    if (!result) return;

    const CliOptions& options = result.value();
    expect_true(options.pid.has_value() && *options.pid == 1234u, "pid should be parsed from -p");
    expect_true(options.processName.has_value() && *options.processName == "notepad.exe", "process name should be parsed from -n");
    expect_true(options.handleType.has_value() && *options.handleType == "File", "handle type should be parsed from -t");
    expect_true(options.objectName.has_value() && *options.objectName == "kernel32", "object name should be parsed from -o");
    expect_true(options.sortBy == SortField::Type, "sort field should be set to type");
    expect_true(options.showCountOnly, "count flag should be enabled");
    expect_true(options.verbose, "verbose flag should be enabled");
}

void test_long_flags_success() {
    auto result = parse_args({"--pid", "777", "--name", "explorer.exe", "--type", "Process", "--object", "token", "--sort", "name", "--count", "--verbose"});
    expect_true(result.has_value(), "long flags should parse successfully");
    if (!result) return;

    const CliOptions& options = result.value();
    expect_true(options.pid.has_value() && *options.pid == 777u, "pid should be parsed from --pid");
    expect_true(options.processName.has_value() && *options.processName == "explorer.exe", "process name should be parsed from --name");
    expect_true(options.handleType.has_value() && *options.handleType == "Process", "handle type should be parsed from --type");
    expect_true(options.objectName.has_value() && *options.objectName == "token", "object name should be parsed from --object");
    expect_true(options.sortBy == SortField::Name, "sort field should be set to name");
    expect_true(options.showCountOnly, "count flag should be enabled with --count");
    expect_true(options.verbose, "verbose flag should be enabled with --verbose");
}

void test_help_flow() {
    auto result = parse_args({"--help"});
    expect_true(!result.has_value(), "help should return unexpected result");
    if (!result) {
        expect_true(result.error().empty(), "help sentinel should be empty string");
    }
}

void test_invalid_pid() {
    auto result = parse_args({"-p", "abc"});
    expect_true(!result.has_value(), "non-numeric pid should fail");
}

void test_unknown_argument() {
    auto result = parse_args({"--does-not-exist"});
    expect_true(!result.has_value(), "unknown argument should fail");
}

void test_duplicate_flags() {
    // Test duplicate PID flags - last one should win
    auto result = parse_args({"-p", "123", "-p", "456"});
    expect_true(result.has_value(), "duplicate flags should parse successfully");
    if (result) {
        expect_true(result->pid.has_value() && *result->pid == 456u, "last PID value should be used");
    }
}

void test_pid_overflow() {
    // Test PID value larger than UINT32_MAX (4294967295)
    auto result = parse_args({"-p", "4294967296"});
    expect_true(!result.has_value(), "PID larger than UINT32_MAX should fail");
}

void test_missing_value_at_end() {
    // Test flag at end without value
    auto result = parse_args({"-p"});
    expect_true(!result.has_value(), "flag without value should fail");
    
    auto result2 = parse_args({"-n"});
    expect_true(!result2.has_value(), "flag without value should fail");
}

void test_multiple_flags_combinations() {
    // Test combining multiple different flags
    auto result = parse_args({"-p", "100", "-v", "-c", "-s", "name"});
    expect_true(result.has_value(), "multiple flag combinations should parse");
    if (result) {
        expect_true(result->pid.has_value() && *result->pid == 100u, "PID should be parsed");
        expect_true(result->verbose, "verbose should be set");
        expect_true(result->showCountOnly, "count flag should be set");
        expect_true(result->sortBy == SortField::Name, "sort field should be name");
    }
}

void test_mixed_short_long_flags() {
    // Test mixing short and long flags
    auto result = parse_args({"--pid", "200", "-v", "--name", "test.exe", "-c"});
    expect_true(result.has_value(), "mixed short/long flags should parse");
    if (result) {
        expect_true(result->pid.has_value() && *result->pid == 200u, "PID from long flag");
        expect_true(result->verbose, "verbose from short flag");
        expect_true(result->processName.has_value() && *result->processName == "test.exe", "name from long flag");
        expect_true(result->showCountOnly, "count from short flag");
    }
}

void test_invalid_sort_field() {
    auto result = parse_args({"-s", "invalid"});
    expect_true(!result.has_value(), "invalid sort field should fail");
}

} // namespace

int main() {
    test_short_flags_success();
    test_long_flags_success();
    test_help_flow();
    test_invalid_pid();
    test_unknown_argument();
    test_duplicate_flags();
    test_pid_overflow();
    test_missing_value_at_end();
    test_multiple_flags_combinations();
    test_mixed_short_long_flags();
    test_invalid_sort_field();

    if (failures == 0) {
        std::cout << "All cli_parser tests passed.\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}
