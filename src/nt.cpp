#include "nt.hpp"

#include <cstddef>
#include <cstring>
#include <limits>

namespace nt {

    // Helper to get the internal NtQuerySystemInformation function from ntdll.dll
    using NtQuerySystemInformationPtr = NTSTATUS(NTAPI*)(
        SYSTEM_INFORMATION_CLASS SystemInformationClass,
        PVOID SystemInformation,
        ULONG SystemInformationLength,
        PULONG ReturnLength
    );

    namespace {

    struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
        PVOID Object;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR HandleValue;
        ULONG GrantedAccess;
        USHORT CreatorBackTraceIndex;
        USHORT ObjectTypeIndex;
        ULONG HandleAttributes;
        ULONG Reserved;
    };

    struct SYSTEM_HANDLE_INFORMATION_EX {
        ULONG_PTR NumberOfHandles;
        ULONG_PTR Reserved;
        SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
    };

    constexpr std::size_t kInitialBufferSize = 1u << 20; // 1 MiB
    constexpr int kMaxRetries = 10;

    [[nodiscard]] std::error_code last_error_code() {
        return std::error_code(static_cast<int>(::GetLastError()), std::system_category());
    }

    [[nodiscard]] std::error_code ntstatus_error(NTSTATUS) {
        return std::make_error_code(std::errc::io_error);
    }

    [[nodiscard]] std::size_t grow_buffer_size(std::size_t current, ULONG needed) {
        std::size_t next = current * 2;
        const std::size_t needed_size = static_cast<std::size_t>(needed);
        if (needed_size > next) {
            next = needed_size + (needed_size / 4);
        }

        if (next < current || next > static_cast<std::size_t>(std::numeric_limits<ULONG>::max())) {
            return static_cast<std::size_t>(std::numeric_limits<ULONG>::max());
        }

        return next;
    }

    [[nodiscard]] bool buffer_has_complete_payload(std::size_t buffer_size, std::size_t handle_count) {
        if (buffer_size < offsetof(SYSTEM_HANDLE_INFORMATION_EX, Handles)) {
            return false;
        }

        const std::size_t header_size = offsetof(SYSTEM_HANDLE_INFORMATION_EX, Handles);
        const std::size_t max_entries = (buffer_size - header_size) / sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX);
        return handle_count <= max_entries;
    }

    } // namespace

    std::expected<void, std::error_code> enable_debug_privilege() {
        HANDLE token_handle = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token_handle)) {
            return std::unexpected(last_error_code());
        }

        LUID luid;
        if (!::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            ::CloseHandle(token_handle);
            return std::unexpected(last_error_code());
        }

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        ::SetLastError(ERROR_SUCCESS);
        if (!::AdjustTokenPrivileges(token_handle, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr)) {
            ::CloseHandle(token_handle);
            return std::unexpected(last_error_code());
        }

        if (::GetLastError() != ERROR_SUCCESS) {
            const auto error = last_error_code();
            ::CloseHandle(token_handle);
            return std::unexpected(error);
        }

        ::CloseHandle(token_handle);
        return {};
    }

    std::expected<std::vector<RawHandle>, std::error_code> query_system_handles() {
        HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) {
            ntdll = ::LoadLibraryW(L"ntdll.dll");
        }

        if (!ntdll) {
            return std::unexpected(last_error_code());
        }

        FARPROC raw_proc = ::GetProcAddress(ntdll, "NtQuerySystemInformation");
        NtQuerySystemInformationPtr nt_query_info = nullptr;
        static_assert(sizeof(nt_query_info) == sizeof(raw_proc));
        std::memcpy(&nt_query_info, &raw_proc, sizeof(nt_query_info));

        if (!nt_query_info) {
            return std::unexpected(std::make_error_code(std::errc::not_supported));
        }

        ULONG needed_size = 0;
        std::vector<std::byte> buffer(kInitialBufferSize);
        NTSTATUS status = 0;

        for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
            status = nt_query_info(
                SystemExtendedHandleInformation,
                buffer.data(),
                static_cast<ULONG>(buffer.size()),
                &needed_size
            );

            if (status == STATUS_SUCCESS) {
                break;
            }

            if (status != STATUS_INFO_LENGTH_MISMATCH) {
                return std::unexpected(ntstatus_error(status));
            }

            const std::size_t next = grow_buffer_size(buffer.size(), needed_size);
            if (next <= buffer.size()) {
                return std::unexpected(std::make_error_code(std::errc::value_too_large));
            }
            buffer.resize(next);
        }

        if (status != STATUS_SUCCESS) {
            return std::unexpected(ntstatus_error(status));
        }

        auto* handle_info = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(buffer.data());
        const std::size_t handle_count = static_cast<std::size_t>(handle_info->NumberOfHandles);

        if (!buffer_has_complete_payload(buffer.size(), handle_count)) {
            return std::unexpected(std::make_error_code(std::errc::result_out_of_range));
        }

        std::vector<RawHandle> result;
        result.reserve(handle_count);

        for (std::size_t i = 0; i < handle_count; ++i) {
            const auto& entry = handle_info->Handles[i];
            result.push_back(RawHandle{
                .objectAddress = reinterpret_cast<std::uintptr_t>(entry.Object),
                .processId = static_cast<std::uintptr_t>(entry.UniqueProcessId),
                .handleValue = static_cast<std::uintptr_t>(entry.HandleValue),
                .grantedAccess = static_cast<std::uint32_t>(entry.GrantedAccess),
                .objectTypeIndex = static_cast<std::uint16_t>(entry.ObjectTypeIndex),
                .handleAttributes = static_cast<std::uint32_t>(entry.HandleAttributes)
            });
        }

        return result;
    }

} // namespace nt