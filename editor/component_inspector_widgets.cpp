#include "../LunaLite/asset/asset_manager.h"
#include "component_inspector_widgets.h"
#include "panels/content_browser_panel.h"

#include <cstdint>

#include <imgui.h>
#include <string>

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
} // namespace

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

bool drawAssetHandleControl(const char* label,
                            scene::Scene& scene,
                            std::string_view commandId,
                            asset::AssetType type,
                            asset::AssetHandle& handle,
                            std::span<const BuiltinAssetOption> builtinOptions)
{
    bool changed = false;
    ImGui::PushID(label);

    uint64_t rawHandle = static_cast<uint64_t>(handle);
    changed |= drawLiveSceneEdit(
        scene,
        commandId,
        [&]() {
            return ImGui::InputScalar(label, ImGuiDataType_U64, &rawHandle);
        },
        [&]() {
            handle = asset::AssetHandle{rawHandle};
        });

    auto droppedHandle = handle;
    if (acceptAssetHandleDrop(type, droppedHandle)) {
        if (actions::beginSceneEdit(scene, commandId)) {
            handle = droppedHandle;
            actions::commitSceneEdit(scene);
        }
        changed = true;
    }

    if (!builtinOptions.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Builtin")) {
            ImGui::OpenPopup("BuiltinAssets");
        }

        if (ImGui::BeginPopup("BuiltinAssets")) {
            for (const auto& option : builtinOptions) {
                if (ImGui::MenuItem(option.label)) {
                    if (actions::beginSceneEdit(scene, commandId)) {
                        handle = option.handle;
                        actions::commitSceneEdit(scene);
                    }
                    changed = true;
                }
            }
            ImGui::EndPopup();
        }
    }

    const auto displayName = getAssetDisplayName(handle);
    ImGui::TextDisabled("%s", displayName.c_str());
    ImGui::PopID();
    return changed;
}

} // namespace lunalite::editor
