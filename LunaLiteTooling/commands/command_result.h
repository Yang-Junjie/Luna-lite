#pragma once

#include "command_value.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace lunalite::tooling {

struct CommandResult {
    bool success{false};
    std::string message;
    std::unordered_map<std::string, CommandValue> values;

    static CommandResult ok(std::string message = {})
    {
        return CommandResult{
            .success = true,
            .message = std::move(message),
        };
    }

    static CommandResult fail(std::string message)
    {
        return CommandResult{
            .success = false,
            .message = std::move(message),
        };
    }

    template <typename T> CommandResult& set(std::string key, T value)
    {
        values[std::move(key)] = CommandValue{std::move(value)};
        return *this;
    }

    template <typename T> const T* get(std::string_view key) const
    {
        const auto value = values.find(std::string{key});
        if (value == values.end()) {
            return nullptr;
        }
        return std::get_if<T>(&value->second);
    }
};

} // namespace lunalite::tooling
