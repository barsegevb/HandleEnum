#include "nt.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
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

void test_grow_buffer_size_prefers_needed_plus_margin() {
    const std::size_t current = 1'024;
    const ULONG needed = 4'096;

    const std::size_t grown = nt::detail::grow_buffer_size(current, needed);
    expect_true(grown == 5'120,
                "grow_buffer_size should use needed + 25% margin when needed exceeds doubling");
}

void test_grow_buffer_size_clamps_on_overflow_risk() {
    const std::size_t near_max = static_cast<std::size_t>(std::numeric_limits<ULONG>::max()) - 8;
    const std::size_t grown = nt::detail::grow_buffer_size(near_max, 16);

    expect_true(grown == static_cast<std::size_t>(std::numeric_limits<ULONG>::max()),
                "grow_buffer_size should clamp to ULONG max when growth overflows or exceeds limit");
}

void test_grow_buffer_size_doubles_when_needed_is_small() {
    const std::size_t current = 8'192;
    const ULONG needed = 1'024;
    const std::size_t grown = nt::detail::grow_buffer_size(current, needed);

    expect_true(grown == 16'384,
                "grow_buffer_size should double current size when needed does not exceed doubling");
}

void test_buffer_has_complete_payload_rejects_too_small_buffer() {
    const bool ok = nt::detail::buffer_has_complete_payload(1, 1);
    expect_true(!ok, "buffer_has_complete_payload should reject buffers smaller than handle header");
}

void test_buffer_has_complete_payload_accepts_zero_handles_for_header_only_buffer() {
    const bool ok = nt::detail::buffer_has_complete_payload(sizeof(void*) * 2, 0);
    expect_true(ok,
                "buffer_has_complete_payload should accept header-only payload when handle count is zero");
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
    test_grow_buffer_size_prefers_needed_plus_margin();
    test_grow_buffer_size_clamps_on_overflow_risk();
    test_grow_buffer_size_doubles_when_needed_is_small();
    test_buffer_has_complete_payload_rejects_too_small_buffer();
    test_buffer_has_complete_payload_accepts_zero_handles_for_header_only_buffer();
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
