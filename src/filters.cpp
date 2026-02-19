#include "filters.hpp"
#include "nt.hpp"

#include <limits>

PidFilter::PidFilter(const uint32_t pid) noexcept
    : m_pid(pid) {}

bool PidFilter::match(const nt::RawHandle& handle) const noexcept {
    if (handle.processId > static_cast<std::uintptr_t>(std::numeric_limits<uint32_t>::max())) {
        return false;
    }

    return static_cast<uint32_t>(handle.processId) == m_pid;
}

// Future location for heavier NtQueryObject-based filters (type/name/access metadata).
