#pragma once

#include "../LunaLite/asset/asset.h"
#include "../LunaLite/asset/asset_types.h"
#include "../LunaLite/scene/scene.h"
#include "editor_actions.h"

#include <imgui.h>
#include <span>
#include <string_view>

namespace lunalite::editor {

struct BuiltinAssetOption {
    const char* label{nullptr};
    asset::AssetHandle handle{0};
};

template <typename DrawFn, typename ApplyFn>
bool drawLiveSceneEdit(scene::Scene& scene, std::string_view commandId, DrawFn&& draw, ApplyFn&& apply)
{
    const bool changed = draw();
    const bool activated = ImGui::IsItemActivated();
    const bool active = ImGui::IsItemActive();
    const bool deactivated = ImGui::IsItemDeactivated();

    if (activated && (active || changed)) {
        actions::beginSceneEdit(scene, commandId);
    } else if (changed && !actions::hasActiveSceneEdit()) {
        actions::beginSceneEdit(scene, commandId);
    }

    if (changed) {
        apply();
    }

    if ((deactivated || (changed && !active)) && actions::hasActiveSceneEdit()) {
        actions::commitSceneEdit(scene);
    }

    return changed;
}

bool acceptAssetHandleDrop(asset::AssetType type, asset::AssetHandle& handle);

bool drawAssetHandleControl(const char* label,
                            scene::Scene& scene,
                            std::string_view commandId,
                            asset::AssetType type,
                            asset::AssetHandle& handle,
                            std::span<const BuiltinAssetOption> builtinOptions);

} // namespace lunalite::editor
