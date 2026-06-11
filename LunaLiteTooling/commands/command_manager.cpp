#include "../../LunaLite/core/log.h"
#include "../../LunaLite/scene/scene_serializer.h"
#include "command_manager.h"
#include "command_registry.h"
#include "../context/tool_context.h"

#include <utility>

namespace lunalite::tooling {

CommandResult CommandManager::execute(std::string_view id, ToolContext& context, const CommandArgs& args)
{
    auto* command = CommandRegistry::get().find(id);
    if (command == nullptr) {
        return CommandResult::fail("Command is not registered");
    }

    const bool record_history = command->canUndo() && context.scene() != nullptr;
    const std::string before_snapshot = record_history ? captureSceneSnapshot(context) : std::string{};

    const auto result = command->execute(context, args);
    if (!result.success) {
        return result;
    }

    if (record_history) {
        m_redo_stack.clear();

        const std::string after_snapshot = captureSceneSnapshot(context);
        if (before_snapshot.empty() || after_snapshot.empty()) {
            LUNA_CORE_WARN("Command '{}' succeeded but history snapshot could not be recorded", id);
            return result;
        }

        m_undo_stack.push_back(HistoryEntry{
            .command_id = std::string{id},
            .before_scene = before_snapshot,
            .after_scene = after_snapshot,
        });
    }

    return result;
}

bool CommandManager::undo(ToolContext& context)
{
    if (m_undo_stack.empty() || context.scene() == nullptr) {
        return false;
    }

    const std::string backup = captureSceneSnapshot(context);
    if (backup.empty()) {
        return false;
    }

    auto entry = std::move(m_undo_stack.back());
    m_undo_stack.pop_back();

    if (!restoreSceneSnapshot(context, entry.before_scene)) {
        restoreSceneSnapshot(context, backup);
        m_undo_stack.push_back(std::move(entry));
        return false;
    }

    m_redo_stack.push_back(std::move(entry));
    return true;
}

bool CommandManager::redo(ToolContext& context)
{
    if (m_redo_stack.empty() || context.scene() == nullptr) {
        return false;
    }

    const std::string backup = captureSceneSnapshot(context);
    if (backup.empty()) {
        return false;
    }

    auto entry = std::move(m_redo_stack.back());
    m_redo_stack.pop_back();

    if (!restoreSceneSnapshot(context, entry.after_scene)) {
        restoreSceneSnapshot(context, backup);
        m_redo_stack.push_back(std::move(entry));
        return false;
    }

    m_undo_stack.push_back(std::move(entry));
    return true;
}

void CommandManager::clearHistory()
{
    m_undo_stack.clear();
    m_redo_stack.clear();
}

bool CommandManager::canUndo() const
{
    return !m_undo_stack.empty();
}

bool CommandManager::canRedo() const
{
    return !m_redo_stack.empty();
}

std::string CommandManager::captureSceneSnapshot(const ToolContext& context) const
{
    const auto* scene = context.scene();
    if (scene == nullptr) {
        return {};
    }

    return scene::SceneSerializer::serializeToString(*scene);
}

bool CommandManager::restoreSceneSnapshot(ToolContext& context, const std::string& snapshot) const
{
    auto* scene = context.scene();
    if (scene == nullptr) {
        return false;
    }

    return scene::SceneSerializer::deserializeFromString(*scene, snapshot);
}

} // namespace lunalite::tooling
