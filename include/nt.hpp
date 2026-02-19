#pragma once

#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <vector>
#include <expected>
#include <system_error>

namespace nt {

    // Status codes for NT API.
    using NTSTATUS = LONG;
    inline constexpr NTSTATUS STATUS_SUCCESS = 0x00000000;
    inline constexpr NTSTATUS STATUS_INFO_LENGTH_MISMATCH = static_cast<NTSTATUS>(0xC0000004u);

    // Use extended handle information for modern 64-bit safe layouts.
    inline constexpr SYSTEM_INFORMATION_CLASS SystemExtendedHandleInformation =
        static_cast<SYSTEM_INFORMATION_CLASS>(64);

    // Public, tool-level representation of one system handle.
    struct RawHandle {
        std::uintptr_t objectAddress{};
        std::uintptr_t processId{};
        std::uintptr_t handleValue{};
        std::uint32_t grantedAccess{};
        std::uint16_t objectTypeIndex{};
        std::uint32_t handleAttributes{};
    };

    namespace detail {
        // Internal helper exposed for deterministic unit testing.
        std::size_t grow_buffer_size(std::size_t current, ULONG needed);
        bool buffer_has_complete_payload(std::size_t buffer_size, std::size_t handle_count);
    }

    /**
     * @brief Elevates the current process privileges to SeDebugPrivilege.
     * @return std::expected<void, std::error_code> Success or error details.
     */
    std::expected<void, std::error_code> enable_debug_privilege();

    /**
     * @brief Retrieves all system handles using NtQuerySystemInformation.
        * @return std::expected<std::vector<RawHandle>, std::error_code> List of handles.
     */
    std::expected<std::vector<RawHandle>, std::error_code> query_system_handles();

} // namespace nt