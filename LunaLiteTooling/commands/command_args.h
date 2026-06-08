#pragma once

#include "command_value.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace lunalite::tooling {

class CommandArgs {
public:
    template <typename T> CommandArgs& set(std::string key, T value)
    {
        m_values[std::move(key)] = CommandValue{std::move(value)};
        return *this;
    }

    template <typename T> std::optional<T> get(std::string_view key) const
    {
        const auto value = m_values.find(std::string{key});
        if (value == m_values.end()) {
            return std::nullopt;
        }

        if (const auto* typedValue = std::get_if<T>(&value->second)) {
            return *typedValue;
        }
        return std::nullopt;
    }

    bool contains(std::string_view key) const
    {
        return m_values.contains(std::string{key});
    }

    const std::unordered_map<std::string, CommandValue>& values() const
    {
        return m_values;
    }

private:
    std::unordered_map<std::string, CommandValue> m_values;
};

} // namespace lunalite::tooling
