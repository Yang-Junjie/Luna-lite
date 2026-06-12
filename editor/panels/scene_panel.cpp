#include "../../LunaLite/asset/asset_manager.h"
#include "../editor_actions.h"
#include "content_browser_panel.h"
#include "scene_panel.h"

#include <cstdint>

#include <imgui.h>
#include <string_view>

namespace lunalite::editor {
namespace {
std::string getAssetDisplayName(asset::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "None";
    }

    if (const auto* metadata = asset::AssetManager::get().getMetadata(handle)) {
        const auto name = metadata->Name.empty() ? metadata->FilePath.filename().string() : metadata->Name;
        return name + " (" + asset::assetTypeToString(metadata->Type) + ")";
    }

    return "Missing asset " + handle.toString();
}

bool acceptAssetHandleDrop(asset::AssetType type, asset::AssetHandle& handle)
{
    bool accepted = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadName)) {
            if (payload->DataSize == sizeof(AssetDragDropPayload)) {
                const auto& assetPayload = *static_cast<const AssetDragDropPayload*>(payload->Data);
                if (assetPayload.type == type && assetPayload.handle.isValid()) {
                    handle = assetPayload.handle;
                    accepted = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
}

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

void drawAssetHandleControl(scene::Scene& scene, const char* label, asset::AssetType type, asset::AssetHandle& handle)
{
    ImGui::PushID(label);

    uint64_t rawHandle = static_cast<uint64_t>(handle);
    drawLiveSceneEdit(
        scene,
        actions::EditSceneSettingsCommandId,
        [&]() {
            return ImGui::InputScalar(label, ImGuiDataType_U64, &rawHandle);
        },
        [&]() {
            handle = asset::AssetHandle{rawHandle};
        });

    auto droppedHandle = handle;
    if (acceptAssetHandleDrop(type, droppedHandle)) {
        if (actions::beginSceneEdit(scene, actions::EditSceneSettingsCommandId)) {
            handle = droppedHandle;
            actions::commitSceneEdit(scene);
        }
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        if (actions::beginSceneEdit(scene, actions::EditSceneSettingsCommandId)) {
            handle = asset::AssetHandle{0};
            actions::commitSceneEdit(scene);
        }
    }

    const auto displayName = getAssetDisplayName(handle);
    ImGui::TextDisabled("%s", displayName.c_str());
    ImGui::PopID();
}
} // namespace

ScenePanel::ScenePanel(scene::Scene& scene)
    : m_scene(scene)
{}

void ScenePanel::onImGuiRender()
{
    ImGui::Begin("Scene");

    auto& settings = m_scene.getSettings();
    drawAssetHandleControl(m_scene, "Environment Map", asset::AssetType::Texture, settings.environment_map);
    auto environmentIntensity = settings.environment_intensity;
    drawLiveSceneEdit(
        m_scene,
        actions::EditSceneSettingsCommandId,
        [&]() {
            return ImGui::DragFloat("Environment Intensity", &environmentIntensity, 0.05f, 0.0f, 1000.0f, "%.3f");
        },
        [&]() {
            settings.environment_intensity = environmentIntensity;
        });

    ImGui::End();
}

} // namespace lunalite::editor
