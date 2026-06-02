#include "../../LunaLite/asset/asset_manager.h"
#include "content_browser_panel.h"
#include "scene_panel.h"

#include <cstdint>

#include <imgui.h>

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

void drawAssetHandleControl(const char* label, asset::AssetType type, asset::AssetHandle& handle)
{
    ImGui::PushID(label);

    uint64_t rawHandle = static_cast<uint64_t>(handle);
    if (ImGui::InputScalar(label, ImGuiDataType_U64, &rawHandle)) {
        handle = asset::AssetHandle{rawHandle};
    }

    acceptAssetHandleDrop(type, handle);

    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        handle = asset::AssetHandle{0};
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
    drawAssetHandleControl("Environment Map", asset::AssetType::Texture, settings.environment_map);
    ImGui::DragFloat("Environment Intensity", &settings.environment_intensity, 0.05f, 0.0f, 1000.0f, "%.3f");

    ImGui::End();
}

} // namespace lunalite::editor
