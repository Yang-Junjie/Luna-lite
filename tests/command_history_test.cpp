#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/scene_components.h"
#include "../LunaLiteTooling/commands/command_manager.h"
#include "../LunaLiteTooling/commands/scene_commands.h"
#include "../LunaLiteTooling/context/tool_context.h"

#include <cstdint>

#include <iostream>
#include <string>
#include <type_traits>

namespace {
using EntityUnderlying = std::underlying_type_t<entt::entity>;

uint64_t entityToValue(lunalite::scene::Entity entity)
{
    return static_cast<uint64_t>(static_cast<EntityUnderlying>(entity.getHandle()));
}

lunalite::scene::Entity entityFromValue(uint64_t value)
{
    return lunalite::scene::Entity{static_cast<entt::entity>(static_cast<EntityUnderlying>(value))};
}
} // namespace

int main()
{
    using namespace lunalite;

    tooling::CommandManager::get().clearHistory();

    scene::Scene scene;
    tooling::ToolContext context;
    context.setScene(scene);

    tooling::CommandArgs rootArgs;
    rootArgs.set("name", std::string{"Root"});
    const auto rootResult = tooling::CommandManager::get().execute(tooling::CreateEntityCommandId, context, rootArgs);
    const auto root = rootResult.get<uint64_t>("created_entity");
    if (!rootResult.success || root == nullptr) {
        std::cerr << "Failed to create root entity.\n";
        return 1;
    }

    tooling::CommandArgs childArgs;
    childArgs.set("name", std::string{"Child"});
    childArgs.set("parent_entity", entityToValue(entityFromValue(*root)));
    const auto childResult = tooling::CommandManager::get().execute(tooling::CreateEntityCommandId, context, childArgs);
    const auto child = childResult.get<uint64_t>("created_entity");
    if (!childResult.success || child == nullptr) {
        std::cerr << "Failed to create child entity.\n";
        return 1;
    }

    tooling::CommandArgs deleteArgs;
    deleteArgs.set("entity", *root);
    const auto deleteResult =
        tooling::CommandManager::get().execute(tooling::DeleteEntityCommandId, context, deleteArgs);
    if (!deleteResult.success) {
        std::cerr << "Failed to delete root entity.\n";
        return 1;
    }
    if (scene.isValidEntity(entityFromValue(*root)) || scene.isValidEntity(entityFromValue(*child))) {
        std::cerr << "Delete command did not remove the expected entities.\n";
        return 1;
    }

    if (!tooling::CommandManager::get().undo(context)) {
        std::cerr << "Undo failed.\n";
        return 1;
    }
    if (!scene.isValidEntity(entityFromValue(*root)) || !scene.isValidEntity(entityFromValue(*child))) {
        std::cerr << "Undo did not restore entities with the original handles.\n";
        return 1;
    }
    if (scene.getParent(entityFromValue(*child)).getHandle() != entityFromValue(*root).getHandle()) {
        std::cerr << "Undo did not restore hierarchy.\n";
        return 1;
    }

    if (!tooling::CommandManager::get().redo(context)) {
        std::cerr << "Redo failed.\n";
        return 1;
    }
    if (scene.isValidEntity(entityFromValue(*root)) || scene.isValidEntity(entityFromValue(*child))) {
        std::cerr << "Redo did not re-apply deletion.\n";
        return 1;
    }

    std::cout << "Command history undo/redo restored scene snapshots.\n";
    return 0;
}
