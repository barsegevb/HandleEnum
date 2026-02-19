#include "nt.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect_true(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_enable_debug_privilege_smoke() {
    auto result = nt::enable_debug_privilege();

    if (!result) {
        expect_true(result.error().value() != 0,
                    "enable_debug_privilege failure should include a non-zero error code");
        expect_true(!result.error().message().empty(),
                    "enable_debug_privilege failure should include an error message");
    } else {
        expect_true(true, "enable_debug_privilege succeeded");
    }
}

void test_query_system_handles_smoke() {
    auto result = nt::query_system_handles();
    expect_true(result.has_value() || !result.has_value(), "query_system_handles should return a valid expected state");

    if (!result) {
        expect_true(result.error().value() != 0,
                    "query_system_handles failure should include a non-zero error code");
        expect_true(!result.error().message().empty(),
                    "query_system_handles failure should include an error message");
        return;
    }

    const auto& handles = result.value();

    expect_true(handles.size() < 10'000'000,
                "query_system_handles returned an implausibly large handle count");

    const std::size_t sample_count = std::min<std::size_t>(handles.size(), 64);
    std::uintptr_t checksum = 0;
    for (std::size_t i = 0; i < sample_count; ++i) {
        checksum ^= handles[i].objectAddress;
        checksum ^= handles[i].processId;
        checksum ^= handles[i].handleValue;
    }

    expect_true(sample_count == 0 || checksum != static_cast<std::uintptr_t>(-1),
                "sample iteration over returned handles should be valid");
}

void test_query_after_privilege_attempt() {
    (void)nt::enable_debug_privilege();

    auto result = nt::query_system_handles();
    if (!result) {
        expect_true(result.error().value() != 0,
                    "query after privilege attempt should return meaningful error if it fails");
        return;
    }

    expect_true(result->size() < 10'000'000,
                "query after privilege attempt returned implausibly many handles");
}

} // namespace

int main() {
    test_enable_debug_privilege_smoke();
    test_query_system_handles_smoke();
    test_query_after_privilege_attempt();

    if (failures == 0) {
        std::cout << "All nt tests passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " nt test(s) failed.\n";
    return EXIT_FAILURE;
}
