#pragma once
#include <cstdint>

#include <string>

namespace lunalite::core {
class UUID {
public:
    UUID();
    explicit UUID(uint64_t uuid);
    UUID(const UUID&) = default;

    explicit operator uint64_t() const
    {
        return m_UUID;
    }

    bool operator==(const UUID& other) const
    {
        return m_UUID == other.m_UUID;
    }

    bool operator!=(const UUID& other) const
    {
        return m_UUID != other.m_UUID;
    }

    std::string toString() const
    {
        return std::to_string(m_UUID);
    }

    bool isValid() const
    {
        return m_UUID != 0;
    }

private:
    uint64_t m_UUID;
};
} // namespace lunalite::core
