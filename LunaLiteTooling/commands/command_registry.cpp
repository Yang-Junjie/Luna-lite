#include "command_registry.h"

#include "../../LunaLite/core/log.h"
#include "asset_commands.h"

#include <utility>

namespace lunalite::tooling {

void CommandRegistry::registerDefaults()
{
    if (m_defaults_registered) {
        return;
    }

    m_defaults_registered = true;
    registerAssetCommands(*this);
}

bool CommandRegistry::registerCommand(CommandDesc command)
{
    if (command.id.empty()) {
        LUNA_CORE_ERROR("Failed to register command: missing id");
        return false;
    }
    if (!command.execute) {
        LUNA_CORE_ERROR("Failed to register command '{}': missing execute handler", command.id);
        return false;
    }
    if (m_commands.contains(command.id)) {
        LUNA_CORE_ERROR("Failed to register command '{}': duplicate id", command.id);
        return false;
    }

    m_commands.emplace(command.id, std::move(command));
    return true;
}

const CommandDesc* CommandRegistry::find(std::string_view id)
{
    registerDefaults();

    const auto command = m_commands.find(std::string{id});
    if (command == m_commands.end()) {
        return nullptr;
    }
    return &command->second;
}

std::vector<const CommandDesc*> CommandRegistry::commands()
{
    registerDefaults();

    std::vector<const CommandDesc*> result;
    result.reserve(m_commands.size());
    for (const auto& [_, command] : m_commands) {
        result.push_back(&command);
    }
    return result;
}

CommandResult CommandRegistry::execute(std::string_view id, ToolContext& context, const CommandArgs& args)
{
    const auto* command = find(id);
    if (command == nullptr) {
        return CommandResult::fail("Command is not registered");
    }

    return command->execute(context, args);
}

} // namespace lunalite::tooling
