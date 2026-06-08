#pragma once

#include "command.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lunalite::tooling {

class CommandRegistry {
public:
    static CommandRegistry& get()
    {
        static CommandRegistry registry;
        return registry;
    }

    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
    CommandRegistry(CommandRegistry&&) = delete;
    CommandRegistry& operator=(CommandRegistry&&) = delete;

    void registerDefaults();
    bool registerCommand(CommandDesc command);
    const CommandDesc* find(std::string_view id);
    std::vector<const CommandDesc*> commands();
    CommandResult execute(std::string_view id, ToolContext& context, const CommandArgs& args = {});

private:
    CommandRegistry() = default;
    ~CommandRegistry() = default;

    bool m_defaults_registered{false};
    std::unordered_map<std::string, CommandDesc> m_commands;
};

} // namespace lunalite::tooling
