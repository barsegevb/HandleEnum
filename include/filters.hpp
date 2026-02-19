#pragma once

#include "nt.hpp"

#include <cstdint>

class IHandleFilter {
public:
    virtual ~IHandleFilter() = default;
    [[nodiscard]] virtual bool match(const nt::RawHandle& handle) const noexcept = 0;
};

class PidFilter final : public IHandleFilter {
public:
    explicit PidFilter(uint32_t pid) noexcept;
    [[nodiscard]] bool match(const nt::RawHandle& handle) const noexcept override;

private:
    uint32_t m_pid;
};
