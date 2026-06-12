#pragma once

#include "command_args.h"
#include "command_result.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lunalite::tooling {

class ToolContext;

class CommandManager {
public:
    static CommandManager& get()
    {
        static CommandManager manager;
        return manager;
    }

    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;
    CommandManager(CommandManager&&) = delete;
    CommandManager& operator=(CommandManager&&) = delete;

    CommandResult execute(std::string_view id, ToolContext& context, const CommandArgs& args = {});
    bool beginSceneEdit(std::string_view id, ToolContext& context);
    bool commitSceneEdit(ToolContext& context);
    bool cancelSceneEdit(ToolContext& context);
    bool undo(ToolContext& context);
    bool redo(ToolContext& context);
    void clearHistory();

    bool hasActiveSceneEdit() const;
    bool canUndo() const;
    bool canRedo() const;

private:
    CommandManager() = default;
    ~CommandManager() = default;

    struct HistoryEntry {
        std::string command_id;
        std::string before_scene;
        std::string after_scene;
    };

    struct SceneEditTransaction {
        std::string command_id;
        std::string before_scene;
    };

    std::string captureSceneSnapshot(const ToolContext& context) const;
    bool restoreSceneSnapshot(ToolContext& context, const std::string& snapshot) const;

    std::vector<HistoryEntry> m_undo_stack;
    std::vector<HistoryEntry> m_redo_stack;
    std::optional<SceneEditTransaction> m_active_scene_edit;
};

} // namespace lunalite::tooling
