#include "filters.hpp"

#include <cstdlib>
#include <expected>
#include <iostream>
#include <string>
#include <system_error>
#include <unordered_map>

namespace {

int failures = 0;

void expect_true(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

std::unordered_map<std::uintptr_t, std::expected<std::string, std::error_code>> g_type_by_handle;
std::unordered_map<std::uintptr_t, std::expected<std::string, std::error_code>> g_name_by_handle;

[[nodiscard]] nt::RawHandle make_handle(const std::uintptr_t value) {
    return nt::RawHandle{
        .processId = 1234,
        .handleValue = value,
        .grantedAccess = 0x1
    };
}

void test_type_filter_case_insensitive_match() {
    g_type_by_handle.clear();
    g_name_by_handle.clear();

    g_type_by_handle.emplace(0x10, std::expected<std::string, std::error_code>(std::string{"Event"}));

    const TypeFilter filter("event");
    expect_true(filter.match(make_handle(0x10)), "TypeFilter should match type in case-insensitive mode");
}

void test_type_filter_non_match() {
    g_type_by_handle.clear();
    g_name_by_handle.clear();

    g_type_by_handle.emplace(0x11, std::expected<std::string, std::error_code>(std::string{"File"}));

    const TypeFilter filter("Process");
    expect_true(!filter.match(make_handle(0x11)), "TypeFilter should reject non-matching types");
}

void test_type_filter_query_error_returns_false() {
    g_type_by_handle.clear();
    g_name_by_handle.clear();

    g_type_by_handle.emplace(0x12, std::unexpected(std::make_error_code(std::errc::io_error)));

    const TypeFilter filter("Event");
    expect_true(!filter.match(make_handle(0x12)), "TypeFilter should return false when query_object_type fails");
}

void test_name_filter_substring_case_insensitive_match() {
    g_type_by_handle.clear();
    g_name_by_handle.clear();

    g_name_by_handle.emplace(0x20, std::expected<std::string, std::error_code>(std::string{"\\Device\\HarddiskVolume3\\Windows\\Temp\\sample.log"}));

    const NameFilter filter("windows\\temp");
    expect_true(filter.match(make_handle(0x20)), "NameFilter should match case-insensitive substring");
}

void test_name_filter_query_error_returns_false() {
    g_type_by_handle.clear();
    g_name_by_handle.clear();

    g_name_by_handle.emplace(0x21, std::unexpected(std::make_error_code(std::errc::permission_denied)));

    const NameFilter filter("Temp");
    expect_true(!filter.match(make_handle(0x21)), "NameFilter should return false when query_object_name fails");
}

} // namespace

namespace nt {

std::expected<std::string, Error> query_object_type(const RawHandle& handle) noexcept {
    if (const auto it = g_type_by_handle.find(handle.handleValue); it != g_type_by_handle.end()) {
        return it->second;
    }

    return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
}

std::expected<std::string, Error> query_object_name(const RawHandle& handle) noexcept {
    if (const auto it = g_name_by_handle.find(handle.handleValue); it != g_name_by_handle.end()) {
        return it->second;
    }

    return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
}

} // namespace nt

int main() {
    test_type_filter_case_insensitive_match();
    test_type_filter_non_match();
    test_type_filter_query_error_returns_false();
    test_name_filter_substring_case_insensitive_match();
    test_name_filter_query_error_returns_false();

    if (failures == 0) {
        std::cout << "All filters tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " filters test(s) failed.\n";
    return EXIT_FAILURE;
}
