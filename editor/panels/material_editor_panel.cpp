#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/renderer/interface/material.h"
#include "../drag_drop.h"
#include "../editor_actions.h"
#include "content_browser_panel.h"
#include "material_editor_panel.h"

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

bool drawAssetHandleControl(const char* label, asset::AssetType type, asset::AssetHandle& handle)
{
    bool changed = false;
    ImGui::PushID(label);

    uint64_t rawHandle = static_cast<uint64_t>(handle);
    if (ImGui::InputScalar(label, ImGuiDataType_U64, &rawHandle)) {
        handle = asset::AssetHandle{rawHandle};
        changed = true;
    }

    if (drag_drop::acceptAssetHandle(type, handle)) {
        changed = true;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        handle = asset::AssetHandle{0};
        changed = true;
    }

    const auto displayName = getAssetDisplayName(handle);
    ImGui::TextDisabled("%s", displayName.c_str());
    ImGui::PopID();
    return changed;
}

} // namespace

void MaterialEditorPanel::onImGuiRender()
{
    ImGui::Begin("Material Editor");

    if (drawAssetHandleControl("Material", asset::AssetType::Material, m_material)) {
        m_dirty = false;
    }
    ImGui::Separator();

    const auto* metadata = m_material.isValid() ? asset::AssetManager::get().getMetadata(m_material) : nullptr;
    if (metadata == nullptr || metadata->Type != asset::AssetType::Material) {
        ImGui::TextUnformatted("No material asset");
        ImGui::End();
        return;
    }

    auto* material = asset::AssetManager::get().getAsset<renderer::interface::Material>(m_material);
    if (material == nullptr || material->parameters == nullptr) {
        ImGui::TextUnformatted("Material asset is not loaded");
        ImGui::End();
        return;
    }

    auto parameters = *material->parameters;
    bool changed = false;

    int shadingModel = parameters.shading_model == renderer::interface::ShadingModel::Unlit ? 1 : 0;
    const char* shadingModels[] = {"Lit", "Unlit"};
    if (ImGui::Combo("Shading Model", &shadingModel, shadingModels, 2)) {
        parameters.shading_model =
            shadingModel == 1 ? renderer::interface::ShadingModel::Unlit : renderer::interface::ShadingModel::Lit;
        changed = true;
    }

    changed |= ImGui::ColorEdit4("Albedo", &parameters.albedo.x);
    changed |= ImGui::DragFloat("Metallic", &parameters.metallic, 0.01f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("Roughness", &parameters.roughness, 0.01f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::ColorEdit3("Emission", &parameters.emission.x);
    changed |= ImGui::DragFloat("Emission Strength", &parameters.emission_strength, 0.05f, 0.0f, 100.0f, "%.3f");
    changed |= ImGui::DragFloat("Normal Scale", &parameters.normal_scale, 0.01f, 0.0f, 4.0f, "%.3f");
    changed |= ImGui::DragFloat("Occlusion Strength", &parameters.occlusion_strength, 0.01f, 0.0f, 1.0f, "%.3f");

    ImGui::Separator();
    changed |= drawAssetHandleControl("Albedo Texture", asset::AssetType::Texture, parameters.albedo_texture);
    changed |= drawAssetHandleControl("Normal Texture", asset::AssetType::Texture, parameters.normal_texture);
    changed |= drawAssetHandleControl(
        "Metallic Roughness Texture", asset::AssetType::Texture, parameters.metallic_roughness_texture);
    changed |= drawAssetHandleControl("Occlusion Texture", asset::AssetType::Texture, parameters.occlusion_texture);
    changed |= drawAssetHandleControl("Emission Texture", asset::AssetType::Texture, parameters.emission_texture);

    if (changed) {
        m_dirty = actions::setMaterialParameters(m_material, parameters) || m_dirty;
    }

    ImGui::Separator();
    if (ImGui::Button("Save")) {
        m_dirty = !actions::saveMaterial(m_material);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_dirty ? "Modified" : "Saved");

    ImGui::End();
}

} // namespace lunalite::editor
