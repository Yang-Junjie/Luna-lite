#include "../LunaLiteTooling/context/selection_context.h"

#include <filesystem>
#include <iostream>

int main()
{
    using namespace lunalite;

    tooling::SelectionContext selection;

    if (selection.hasSelection() || selection.kind() != tooling::SelectionKind::None) {
        std::cerr << "Selection should start empty.\n";
        return 1;
    }

    selection.selectEntity(scene::Entity{entt::entity{7}});
    if (!selection.isEntity() || selection.selectedEntity().getHandle() != entt::entity{7}) {
        std::cerr << "Entity selection was not stored correctly.\n";
        return 1;
    }

    selection.selectAsset(asset::AssetHandle{123});
    if (!selection.isAsset() || selection.selectedAsset() != asset::AssetHandle{123}) {
        std::cerr << "Asset selection was not stored correctly.\n";
        return 1;
    }

    selection.selectFolder(std::filesystem::path{"Assets/Sprites"});
    if (!selection.isFolder() || selection.selectedPath() != std::filesystem::path{"Assets/Sprites"}) {
        std::cerr << "Folder selection was not stored correctly.\n";
        return 1;
    }

    selection.selectProject();
    if (!selection.isProject()) {
        std::cerr << "Project selection was not stored correctly.\n";
        return 1;
    }

    selection.clear();
    if (selection.hasSelection() || selection.kind() != tooling::SelectionKind::None) {
        std::cerr << "Selection should clear back to empty.\n";
        return 1;
    }

    std::cout << "SelectionContext stores editor selection state.\n";
    return 0;
}
