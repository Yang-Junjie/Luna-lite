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
        return m_uuid;
    }

    bool operator==(const UUID& other) const
    {
        return m_uuid == other.m_uuid;
    }

    bool operator!=(const UUID& other) const
    {
        return m_uuid != other.m_uuid;
    }

    std::string toString() const
    {
        return std::to_string(m_uuid);
    }

    bool isValid() const
    {
        return m_uuid != 0;
    }

private:
    uint64_t m_uuid;
};
} // namespace lunalite::core
