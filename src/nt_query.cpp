#include "nt.hpp"

#include "string_utils.hpp"

#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace nt {

using NtQueryObjectPtr = NTSTATUS(NTAPI*)(
    HANDLE Handle,
    OBJECT_INFORMATION_CLASS ObjectInformationClass,
    PVOID ObjectInformation,
    ULONG ObjectInformationLength,
    PULONG ReturnLength
);

namespace {

constexpr ULONG kObjectTypeInformation = 2;
constexpr ULONG kObjectNameInformation = 1;
constexpr int kMaxRetries = 10;

struct OBJECT_TYPE_INFORMATION_HEAD {
    UNICODE_STRING TypeName;
};

struct OBJECT_NAME_INFORMATION_HEAD {
    UNICODE_STRING Name;
};

[[nodiscard]] std::error_code last_error_code() {
    return std::error_code(static_cast<int>(::GetLastError()), std::system_category());
}

[[nodiscard]] std::error_code ntstatus_error(NTSTATUS) {
    return std::make_error_code(std::errc::io_error);
}

[[nodiscard]] std::expected<std::string, Error> make_error(const std::errc errc) {
    return std::unexpected(std::make_error_code(errc));
}

[[nodiscard]] NtQueryObjectPtr load_nt_query_object() {
    static NtQueryObjectPtr cached_ptr = []() -> NtQueryObjectPtr {
        HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) {
            ntdll = ::LoadLibraryW(L"ntdll.dll");
        }

        if (!ntdll) {
            return nullptr;
        }

        FARPROC raw_proc = ::GetProcAddress(ntdll, "NtQueryObject");
        NtQueryObjectPtr ptr = nullptr;
        std::memcpy(&ptr, &raw_proc, sizeof(ptr));
        return ptr;
    }();

    return cached_ptr;
}

[[nodiscard]] bool looks_like_sync_pipe_file(const RawHandle& handle) {
    constexpr std::uint32_t pipe_mask = FILE_READ_DATA | FILE_WRITE_DATA | SYNCHRONIZE;
    return (handle.grantedAccess & pipe_mask) == pipe_mask;
}

[[nodiscard]] std::expected<HANDLE, Error> duplicate_to_current_process(const RawHandle& handle) {
    if (handle.processId == 0 || handle.handleValue == 0) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    if (handle.processId > static_cast<std::uintptr_t>(std::numeric_limits<DWORD>::max())) {
        return std::unexpected(std::make_error_code(std::errc::result_out_of_range));
    }

    const DWORD source_pid = static_cast<DWORD>(handle.processId);
    HANDLE source_process = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, source_pid);
    if (!source_process) {
        return std::unexpected(last_error_code());
    }

    HANDLE duplicated = nullptr;
    const BOOL duplicated_ok = ::DuplicateHandle(
        source_process,
        reinterpret_cast<HANDLE>(handle.handleValue),
        ::GetCurrentProcess(),
        &duplicated,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS
    );

    ::CloseHandle(source_process);

    if (!duplicated_ok || !duplicated) {
        return std::unexpected(last_error_code());
    }

    return duplicated;
}

[[nodiscard]] std::expected<std::string, Error> query_unicode_information(
    NtQueryObjectPtr nt_query_object,
    HANDLE duplicated,
    const OBJECT_INFORMATION_CLASS info_class
) {
    ULONG needed_size = 0;
    NTSTATUS status = nt_query_object(
        duplicated,
        info_class,
        nullptr,
        0,
        &needed_size
    );

    if (status != STATUS_INFO_LENGTH_MISMATCH && status != STATUS_SUCCESS) {
        return std::unexpected(ntstatus_error(status));
    }

    std::size_t buffer_size = (needed_size == 0) ? 512u : static_cast<std::size_t>(needed_size);
    if (buffer_size > static_cast<std::size_t>(std::numeric_limits<ULONG>::max())) {
        return std::unexpected(std::make_error_code(std::errc::value_too_large));
    }

    std::vector<std::byte> buffer(buffer_size);
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
        needed_size = 0;
        status = nt_query_object(
            duplicated,
            info_class,
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

        const std::size_t next = detail::grow_buffer_size(buffer.size(), needed_size);
        if (next <= buffer.size()) {
            return std::unexpected(std::make_error_code(std::errc::value_too_large));
        }
        buffer.resize(next);
    }

    if (status != STATUS_SUCCESS) {
        return std::unexpected(ntstatus_error(status));
    }

    const UNICODE_STRING* unicode = nullptr;
    if (info_class == static_cast<OBJECT_INFORMATION_CLASS>(kObjectTypeInformation)) {
        const auto* info = reinterpret_cast<const OBJECT_TYPE_INFORMATION_HEAD*>(buffer.data());
        unicode = &info->TypeName;
    } else {
        const auto* info = reinterpret_cast<const OBJECT_NAME_INFORMATION_HEAD*>(buffer.data());
        unicode = &info->Name;
    }

    if (!unicode || unicode->Buffer == nullptr || unicode->Length == 0) {
        return std::string{};
    }

    return utils::utf16_to_utf8(std::wstring_view(unicode->Buffer, unicode->Length / sizeof(wchar_t)));
}

} // namespace

std::expected<std::string, Error> query_object_type(const RawHandle& handle) noexcept {
    try {
        const NtQueryObjectPtr nt_query_object = load_nt_query_object();
        if (!nt_query_object) {
            return make_error(std::errc::not_supported);
        }

        auto duplicated_result = duplicate_to_current_process(handle);
        if (!duplicated_result) {
            return std::unexpected(duplicated_result.error());
        }

        const HANDLE duplicated = *duplicated_result;
        auto type_result = query_unicode_information(
            nt_query_object,
            duplicated,
            static_cast<OBJECT_INFORMATION_CLASS>(kObjectTypeInformation)
        );

        ::CloseHandle(duplicated);
        return type_result;
    } catch (const std::bad_alloc&) {
        return make_error(std::errc::not_enough_memory);
    } catch (...) {
        return make_error(std::errc::io_error);
    }
}

std::expected<std::string, Error> query_object_name(const RawHandle& handle) noexcept {
    try {
        if (handle.grantedAccess == 0) {
            return make_error(std::errc::permission_denied);
        }

        if (looks_like_sync_pipe_file(handle)) {
            return make_error(std::errc::operation_would_block);
        }

        const NtQueryObjectPtr nt_query_object = load_nt_query_object();
        if (!nt_query_object) {
            return make_error(std::errc::not_supported);
        }

        auto duplicated_result = duplicate_to_current_process(handle);
        if (!duplicated_result) {
            return std::unexpected(duplicated_result.error());
        }

        const HANDLE duplicated = *duplicated_result;

        auto type_result = query_unicode_information(
            nt_query_object,
            duplicated,
            static_cast<OBJECT_INFORMATION_CLASS>(kObjectTypeInformation)
        );
        if (!type_result) {
            ::CloseHandle(duplicated);
            return std::unexpected(type_result.error());
        }

        if (*type_result == "File" && looks_like_sync_pipe_file(handle)) {
            ::CloseHandle(duplicated);
            return make_error(std::errc::operation_would_block);
        }

        auto name_result = query_unicode_information(
            nt_query_object,
            duplicated,
            static_cast<OBJECT_INFORMATION_CLASS>(kObjectNameInformation)
        );

        ::CloseHandle(duplicated);
        return name_result;
    } catch (const std::bad_alloc&) {
        return make_error(std::errc::not_enough_memory);
    } catch (...) {
        return make_error(std::errc::io_error);
    }
}

} // namespace nt
