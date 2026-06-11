#include "../../LunaLite/core/log.h"
#include "asset_commands.h"
#include "command_registry.h"
#include "scene_commands.h"

#include <memory>
#include <string>
#include <utility>

namespace lunalite::tooling {

void CommandRegistry::registerDefaults()
{
    if (m_defaults_registered) {
        return;
    }

    m_defaults_registered = true;
    registerAssetCommands(*this);
    registerSceneCommands(*this);
}

bool CommandRegistry::registerCommand(std::unique_ptr<Command> command)
{
    if (command == nullptr) {
        LUNA_CORE_ERROR("Failed to register command: null command");
        return false;
    }

    const auto id = command->id();
    if (id.empty()) {
        LUNA_CORE_ERROR("Failed to register command: missing id");
        return false;
    }

    const auto key = std::string{id};
    if (m_commands.contains(key)) {
        LUNA_CORE_ERROR("Failed to register command '{}': duplicate id", id);
        return false;
    }

    m_commands.emplace(std::move(key), std::move(command));
    return true;
}

Command* CommandRegistry::find(std::string_view id)
{
    registerDefaults();

    const auto command = m_commands.find(std::string{id});
    if (command == m_commands.end()) {
        return nullptr;
    }
    return command->second.get();
}

std::vector<Command*> CommandRegistry::commands()
{
    registerDefaults();

    std::vector<Command*> result;
    result.reserve(m_commands.size());
    for (const auto& [_, command] : m_commands) {
        result.push_back(command.get());
    }
    return result;
}

CommandResult CommandRegistry::execute(std::string_view id, ToolContext& context, const CommandArgs& args)
{
    auto* command = find(id);
    if (command == nullptr) {
        return CommandResult::fail("Command is not registered");
    }

    return command->execute(context, args);
}

} // namespace lunalite::tooling
