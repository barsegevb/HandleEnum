#include "app.hpp"
#include "nt.hpp"

#include <cstdlib>
#include <expected>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

struct NtStubConfig {
    bool privilege_ok = true;
    bool query_ok = true;
    std::size_t handle_count = 3;
    std::error_code privilege_error = std::make_error_code(std::errc::operation_not_permitted);
    std::error_code query_error = std::make_error_code(std::errc::io_error);
};

NtStubConfig g_nt_stub_config{};

struct RunResult {
    int exit_code = EXIT_FAILURE;
    std::string out;
    std::string err;
};

RunResult run_app(std::initializer_list<const char*> args) {
    std::vector<std::string> owned;
    owned.reserve(args.size() + 1);
    owned.emplace_back("HandleEnum.exe");
    for (const char* arg : args) {
        owned.emplace_back(arg);
    }

    std::vector<char*> argv;
    argv.reserve(owned.size());
    for (std::string& item : owned) {
        argv.push_back(item.data());
    }

    std::ostringstream out_capture;
    std::ostringstream err_capture;

    auto* old_out = std::cout.rdbuf(out_capture.rdbuf());
    auto* old_err = std::cerr.rdbuf(err_capture.rdbuf());

    const int code = HandleEnumApp{}.run(static_cast<int>(argv.size()), argv.data());

    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    return RunResult{code, out_capture.str(), err_capture.str()};
}

void test_help_returns_success() {
    g_nt_stub_config = {};
    const auto result = run_app({"--help"});

    expect_true(result.exit_code == EXIT_SUCCESS, "--help should return success");
    expect_true(result.out.find("HandleEnum.exe [OPTIONS]") != std::string::npos,
                "--help should print usage text");
}

void test_invalid_argument_returns_failure() {
    g_nt_stub_config = {};
    const auto result = run_app({"--not-real"});

    expect_true(result.exit_code == EXIT_FAILURE, "invalid argument should return failure");
    expect_true(result.err.find("Error:") != std::string::npos,
                "invalid argument should print error message");
}

void test_successful_flow_prints_summary() {
    g_nt_stub_config = {};
    g_nt_stub_config.query_ok = true;
    g_nt_stub_config.handle_count = 5;

    const auto result = run_app({"-v", "-p", "1234"});

    expect_true(result.exit_code == EXIT_SUCCESS, "valid args with successful NT calls should return success");
    expect_true(result.out.find("Verbose mode is ON") != std::string::npos,
                "verbose mode should be printed");
    expect_true(result.out.find("Filtering by PID: 1234") != std::string::npos,
                "pid filter should be printed");
    expect_true(result.out.find("Retrieved 5 system handles.") != std::string::npos,
                "summary should include queried handle count");
}

void test_query_failure_returns_failure() {
    g_nt_stub_config = {};
    g_nt_stub_config.query_ok = false;

    const auto result = run_app({"-v"});

    expect_true(result.exit_code == EXIT_FAILURE, "query failure should return failure");
    expect_true(result.err.find("failed to query system handles") != std::string::npos,
                "query failure should print query error message");
}

void test_privilege_failure_only_warns() {
    g_nt_stub_config = {};
    g_nt_stub_config.privilege_ok = false;
    g_nt_stub_config.query_ok = true;
    g_nt_stub_config.handle_count = 2;

    const auto result = run_app({"-v"});

    expect_true(result.exit_code == EXIT_SUCCESS,
                "privilege failure should warn but still succeed when query succeeds");
    expect_true(result.err.find("Warning: failed to enable SeDebugPrivilege") != std::string::npos,
                "privilege failure should emit warning");
    expect_true(result.out.find("Retrieved 2 system handles.") != std::string::npos,
                "query success path should still print summary");
}

} // namespace

namespace nt {

std::expected<void, std::error_code> enable_debug_privilege() {
    if (!g_nt_stub_config.privilege_ok) {
        return std::unexpected(g_nt_stub_config.privilege_error);
    }
    return {};
}

std::expected<std::vector<RawHandle>, std::error_code> query_system_handles() {
    if (!g_nt_stub_config.query_ok) {
        return std::unexpected(g_nt_stub_config.query_error);
    }

    std::vector<RawHandle> handles;
    handles.resize(g_nt_stub_config.handle_count);
    return handles;
}

std::expected<std::string, Error> query_object_type(const RawHandle&) noexcept {
    return std::unexpected(std::make_error_code(std::errc::not_supported));
}

std::expected<std::string, Error> query_object_name(const RawHandle&) noexcept {
    return std::unexpected(std::make_error_code(std::errc::not_supported));
}

} // namespace nt

int main() {
    test_help_returns_success();
    test_invalid_argument_returns_failure();
    test_successful_flow_prints_summary();
    test_query_failure_returns_failure();
    test_privilege_failure_only_warns();

    if (failures == 0) {
        std::cout << "All app tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " app test(s) failed.\n";
    return EXIT_FAILURE;
}
