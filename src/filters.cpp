#include "filters.hpp"
#include "nt.hpp"
#include "string_utils.hpp"

#include <limits>
#include <string>

PidFilter::PidFilter(const uint32_t pid) noexcept
    : m_pid(pid) {}

bool PidFilter::match(const nt::RawHandle& handle) const noexcept {
    if (handle.processId > static_cast<std::uintptr_t>(std::numeric_limits<uint32_t>::max())) {
        return false;
    }

    return static_cast<uint32_t>(handle.processId) == m_pid;
}

TypeFilter::TypeFilter(std::string targetType)
    : m_targetType(std::move(targetType)) {}

bool TypeFilter::match(const nt::RawHandle& handle) const noexcept {
    const auto type_result = nt::query_object_type(handle);
    if (!type_result) {
        return false;
    }

    return utils::equals_ignore_case(*type_result, m_targetType);
}

NameFilter::NameFilter(std::string targetName)
    : m_targetName(std::move(targetName)) {}

bool NameFilter::match(const nt::RawHandle& handle) const noexcept {
    const auto name_result = nt::query_object_name(handle);
    if (!name_result) {
        return false;
    }

    return utils::contains_ignore_case(*name_result, m_targetName);
}

// Future location for heavier NtQueryObject-based filters (type/name/access metadata).
