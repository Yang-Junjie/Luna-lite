#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/project/project_manager.h"
#include "../../LunaLite/renderer/interface/material.h"
#include "content_browser_panel.h"
#include "material_editor_panel.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <system_error>

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

bool drawAssetHandleControl(const char* label, asset::AssetType type, asset::AssetHandle& handle)
{
    bool changed = false;
    ImGui::PushID(label);

    uint64_t rawHandle = static_cast<uint64_t>(handle);
    if (ImGui::InputScalar(label, ImGuiDataType_U64, &rawHandle)) {
        handle = asset::AssetHandle{rawHandle};
        changed = true;
    }

    if (acceptAssetHandleDrop(type, handle)) {
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

const char* shadingModelToString(renderer::interface::ShadingModel shadingModel)
{
    switch (shadingModel) {
        case renderer::interface::ShadingModel::Unlit:
            return "Unlit";
        case renderer::interface::ShadingModel::Lit:
        default:
            return "Lit";
    }
}

bool saveMaterial(const asset::AssetMetadata& metadata, const renderer::interface::MaterialParameters& parameters)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        LUNA_CORE_ERROR("Failed to create material directory '{}': {}", path.parent_path().string(), error.message());
        return false;
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open material file for writing: '{}'", path.string());
        return false;
    }

    out << "Material:\n";
    out << "  ShadingModel: " << shadingModelToString(parameters.shading_model) << "\n";
    out << "  Albedo: [" << parameters.albedo.r << ", " << parameters.albedo.g << ", " << parameters.albedo.b << ", "
        << parameters.albedo.a << "]\n";
    out << "  Metallic: " << parameters.metallic << "\n";
    out << "  Roughness: " << parameters.roughness << "\n";
    out << "  Emission: [" << parameters.emission.r << ", " << parameters.emission.g << ", " << parameters.emission.b
        << "]\n";
    out << "  EmissionStrength: " << parameters.emission_strength << "\n";
    out << "  NormalScale: " << parameters.normal_scale << "\n";
    out << "  OcclusionStrength: " << parameters.occlusion_strength << "\n";
    out << "  Textures:\n";
    out << "    Albedo: " << static_cast<uint64_t>(parameters.albedo_texture) << "\n";
    out << "    Normal: " << static_cast<uint64_t>(parameters.normal_texture) << "\n";
    out << "    MetallicRoughness: " << static_cast<uint64_t>(parameters.metallic_roughness_texture) << "\n";
    out << "    Occlusion: " << static_cast<uint64_t>(parameters.occlusion_texture) << "\n";
    out << "    Emission: " << static_cast<uint64_t>(parameters.emission_texture) << "\n";

    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write material file: '{}'", path.string());
        return false;
    }

    return true;
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

    auto& parameters = *material->parameters;
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

    parameters.metallic = std::clamp(parameters.metallic, 0.0f, 1.0f);
    parameters.roughness = std::clamp(parameters.roughness, 0.0f, 1.0f);
    parameters.emission_strength = std::max(parameters.emission_strength, 0.0f);
    parameters.normal_scale = std::max(parameters.normal_scale, 0.0f);
    parameters.occlusion_strength = std::clamp(parameters.occlusion_strength, 0.0f, 1.0f);

    ImGui::Separator();
    changed |= drawAssetHandleControl("Albedo Texture", asset::AssetType::Texture, parameters.albedo_texture);
    changed |= drawAssetHandleControl("Normal Texture", asset::AssetType::Texture, parameters.normal_texture);
    changed |= drawAssetHandleControl(
        "Metallic Roughness Texture", asset::AssetType::Texture, parameters.metallic_roughness_texture);
    changed |= drawAssetHandleControl("Occlusion Texture", asset::AssetType::Texture, parameters.occlusion_texture);
    changed |= drawAssetHandleControl("Emission Texture", asset::AssetType::Texture, parameters.emission_texture);

    if (changed) {
        m_dirty = true;
    }

    ImGui::Separator();
    if (ImGui::Button("Save")) {
        m_dirty = !saveMaterial(*metadata, parameters);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_dirty ? "Modified" : "Saved");

    ImGui::End();
}

} // namespace lunalite::editor
