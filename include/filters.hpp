#pragma once

#include "nt.hpp"

#include <cstdint>
#include <string>

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

class TypeFilter final : public IHandleFilter {
public:
    explicit TypeFilter(std::string targetType);
    [[nodiscard]] bool match(const nt::RawHandle& handle) const noexcept override;

private:
    std::string m_targetType;
};

class NameFilter final : public IHandleFilter {
public:
    explicit NameFilter(std::string targetName);
    [[nodiscard]] bool match(const nt::RawHandle& handle) const noexcept override;

private:
    std::string m_targetName;
};
