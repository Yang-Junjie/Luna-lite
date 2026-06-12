#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLiteTooling/commands/command_manager.h"
#include "../LunaLiteTooling/context/tool_context.h"

#include <glm/glm.hpp>
#include <iostream>

int main()
{
    using namespace lunalite;

    tooling::CommandManager::get().clearHistory();

    scene::Scene scene;
    const auto entity = scene.createEntity();

    tooling::ToolContext context;
    context.setScene(scene);

    if (!tooling::CommandManager::get().beginSceneEdit("scene.edit_transform", context)) {
        std::cerr << "Failed to begin scene edit transaction.\n";
        return 1;
    }

    scene.getComponent<scene::TransformComponent>(entity).translation = glm::vec3{1.0f, 2.0f, 3.0f};
    if (scene.getComponent<scene::TransformComponent>(entity).translation != glm::vec3{1.0f, 2.0f, 3.0f}) {
        std::cerr << "Scene edit did not update the scene immediately.\n";
        return 1;
    }

    if (!tooling::CommandManager::get().commitSceneEdit(context)) {
        std::cerr << "Failed to commit scene edit transaction.\n";
        return 1;
    }
    if (!tooling::CommandManager::get().canUndo()) {
        std::cerr << "Committed scene edit was not added to undo history.\n";
        return 1;
    }

    if (!tooling::CommandManager::get().undo(context)) {
        std::cerr << "Failed to undo scene edit transaction.\n";
        return 1;
    }
    if (scene.getComponent<scene::TransformComponent>(entity).translation != glm::vec3{0.0f}) {
        std::cerr << "Undo did not restore the scene edit snapshot.\n";
        return 1;
    }

    if (!tooling::CommandManager::get().redo(context)) {
        std::cerr << "Failed to redo scene edit transaction.\n";
        return 1;
    }
    if (scene.getComponent<scene::TransformComponent>(entity).translation != glm::vec3{1.0f, 2.0f, 3.0f}) {
        std::cerr << "Redo did not restore the edited scene snapshot.\n";
        return 1;
    }

    tooling::CommandManager::get().clearHistory();
    if (!tooling::CommandManager::get().beginSceneEdit("scene.edit_transform", context) ||
        !tooling::CommandManager::get().commitSceneEdit(context)) {
        std::cerr << "Failed to commit unchanged scene edit transaction.\n";
        return 1;
    }
    if (tooling::CommandManager::get().canUndo()) {
        std::cerr << "Unchanged scene edit should not add undo history.\n";
        return 1;
    }

    if (!tooling::CommandManager::get().beginSceneEdit("scene.edit_transform", context)) {
        std::cerr << "Failed to begin cancellable scene edit transaction.\n";
        return 1;
    }
    scene.getComponent<scene::TransformComponent>(entity).scale = glm::vec3{2.0f};
    if (!tooling::CommandManager::get().cancelSceneEdit(context)) {
        std::cerr << "Failed to cancel scene edit transaction.\n";
        return 1;
    }
    if (scene.getComponent<scene::TransformComponent>(entity).scale != glm::vec3{1.0f}) {
        std::cerr << "Cancel did not restore the pre-edit scene snapshot.\n";
        return 1;
    }

    std::cout << "Scene edit transactions record live edits as a single undoable history entry.\n";
    return 0;
}
