#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/builtin/builtin_assets.h"
#include "../../LunaLite/renderer/interface/mesh.h"
#include "../../LunaLite/scene/components.h"
#include "content_browser_panel.h"
#include "inspector_panel.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace lunalite::editor {
namespace {
struct BuiltinAssetOption {
    const char* label{nullptr};
    asset::AssetHandle handle{0};
};

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

bool drawAssetHandleControl(const char* label,
                            asset::AssetType type,
                            asset::AssetHandle& handle,
                            std::span<const BuiltinAssetOption> builtinOptions)
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

    if (!builtinOptions.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Builtin")) {
            ImGui::OpenPopup("BuiltinAssets");
        }

        if (ImGui::BeginPopup("BuiltinAssets")) {
            for (const auto& option : builtinOptions) {
                if (ImGui::MenuItem(option.label)) {
                    handle = option.handle;
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

float quatLengthSquared(const glm::quat& rotation)
{
    return rotation.w * rotation.w + rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z;
}

glm::quat safeNormalize(glm::quat rotation)
{
    if (quatLengthSquared(rotation) <= 0.000001f) {
        return glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    }

    return glm::normalize(rotation);
}

bool sameOrientation(const glm::quat& lhs, const glm::quat& rhs)
{
    const auto a = safeNormalize(lhs);
    const auto b = safeNormalize(rhs);
    return std::abs(glm::dot(a, b)) > 0.99999f;
}

bool drawShadowMapSizeControl(uint32_t& map_size)
{
    static constexpr std::array<uint32_t, 5> mapSizeOptions{512u, 1'024u, 2'048u, 4'096u, 8'192u};

    bool changed = false;
    const auto preview = std::to_string(map_size);
    if (ImGui::BeginCombo("Map Size", preview.c_str())) {
        for (const auto option : mapSizeOptions) {
            const bool selected = map_size == option;
            const auto label = std::to_string(option);
            if (ImGui::Selectable(label.c_str(), selected)) {
                map_size = option;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}
} // namespace

InspectorPanel::InspectorPanel(scene::Scene& scene, scene::Entity& selected_entity)
    : m_scene(scene),
      m_selected_entity(selected_entity)
{}

void InspectorPanel::onImGuiRender()
{
    ImGui::Begin("Inspector");

    if (!m_scene.isValidEntity(m_selected_entity)) {
        ImGui::TextUnformatted("No entity selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity %u", static_cast<uint32_t>(m_selected_entity.getHandle()));
    ImGui::Separator();

    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponent");
    }

    if (ImGui::BeginPopup("AddComponent")) {
        if (!m_scene.hasComponent<scene::MeshRendererComponent>(m_selected_entity) &&
            ImGui::MenuItem("Mesh Renderer")) {
            m_scene.addComponent<scene::MeshRendererComponent>(m_selected_entity);
        }

        if (!m_scene.hasComponent<scene::ScriptComponent>(m_selected_entity) && ImGui::MenuItem("Script")) {
            m_scene.addComponent<scene::ScriptComponent>(m_selected_entity);
        }

        if (!m_scene.hasComponent<scene::CameraComponent>(m_selected_entity) && ImGui::MenuItem("Camera")) {
            m_scene.addComponent<scene::CameraComponent>(m_selected_entity);
        }

        if (!m_scene.hasComponent<scene::DirectionalLightComponent>(m_selected_entity) &&
            ImGui::MenuItem("Directional Light")) {
            m_scene.addComponent<scene::DirectionalLightComponent>(m_selected_entity);
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    if (m_scene.hasComponent<scene::TagComponent>(m_selected_entity)) {
        const bool open = ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen);
        if (open && m_scene.hasComponent<scene::TagComponent>(m_selected_entity)) {
            auto& tag = m_scene.getComponent<scene::TagComponent>(m_selected_entity);
            std::array<char, 256> buffer{};
            const size_t copySize = tag.tag.size() < buffer.size() - 1 ? tag.tag.size() : buffer.size() - 1;
            std::memcpy(buffer.data(), tag.tag.data(), copySize);
            if (ImGui::InputText("Name", buffer.data(), buffer.size())) {
                tag.tag = buffer.data();
            }
        }
    }

    if (m_scene.hasComponent<scene::TransformComponent>(m_selected_entity)) {
        const bool open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
        if (open && m_scene.hasComponent<scene::TransformComponent>(m_selected_entity)) {
            auto& transform = m_scene.getComponent<scene::TransformComponent>(m_selected_entity);
            ImGui::DragFloat3("Translation", &transform.translation.x, 0.1f);
            syncRotationEditor(m_selected_entity, transform.rotation);
            auto rotation = m_rotation_edit_degrees;
            if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) {
                const auto delta = rotation - m_rotation_edit_degrees;
                transform.rotation = safeNormalize(transform.rotation * glm::quat{glm::radians(delta)});
                m_rotation_edit_degrees = rotation;
                m_rotation_edit_source = transform.rotation;
            }
            ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f);
        }
    }

    if (m_scene.hasComponent<scene::MeshRendererComponent>(m_selected_entity)) {
        const bool open = ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("MeshRendererPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                m_scene.removeComponent<scene::MeshRendererComponent>(m_selected_entity);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::MeshRendererComponent>(m_selected_entity)) {
            auto& meshRenderer = m_scene.getComponent<scene::MeshRendererComponent>(m_selected_entity);
            const BuiltinAssetOption builtinMeshes[] = {
                {"Cube", asset::builtin::cubeMeshHandle()},
                {"Plane", asset::builtin::planeMeshHandle()},
            };
            const BuiltinAssetOption builtinMaterials[] = {
                {"Default", asset::builtin::defaultMaterialHandle()},
                {"Error", asset::builtin::errorMaterialHandle()},
            };

            drawAssetHandleControl("Mesh", asset::AssetType::Mesh, meshRenderer.mesh, builtinMeshes);

            auto* meshAsset = meshRenderer.mesh.isValid()
                                  ? asset::AssetManager::get().getAsset<renderer::interface::Mesh>(meshRenderer.mesh)
                                  : nullptr;
            if (meshAsset == nullptr) {
                ImGui::TextDisabled("No mesh asset loaded");
            } else {
                ImGui::TextDisabled("Submeshes: %zu", meshAsset->getSubMeshes().size());
            }

            if (ImGui::Button("Add Material")) {
                meshRenderer.materials.push_back(asset::builtin::defaultMaterialHandle());
            }

            int materialToDelete = -1;
            for (size_t materialIndex = 0; materialIndex < meshRenderer.materials.size(); ++materialIndex) {
                ImGui::PushID(static_cast<int>(materialIndex));
                ImGui::Text("Material %zu", materialIndex);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    materialToDelete = static_cast<int>(materialIndex);
                }
                drawAssetHandleControl(
                    "Material", asset::AssetType::Material, meshRenderer.materials[materialIndex], builtinMaterials);
                ImGui::PopID();
            }

            if (materialToDelete >= 0) {
                meshRenderer.materials.erase(meshRenderer.materials.begin() + materialToDelete);
            }
        }
    }

    if (m_scene.hasComponent<scene::ScriptComponent>(m_selected_entity)) {
        const bool open = ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("ScriptPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                m_scene.removeComponent<scene::ScriptComponent>(m_selected_entity);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::ScriptComponent>(m_selected_entity)) {
            auto& script = m_scene.getComponent<scene::ScriptComponent>(m_selected_entity);
            if (ImGui::Button("Add Script")) {
                script.scripts.push_back({});
            }

            int scriptToDelete = -1;
            for (size_t i = 0; i < script.scripts.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::Separator();
                ImGui::Text("Script %zu", i);
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete")) {
                    scriptToDelete = static_cast<int>(i);
                }

                auto& binding = script.scripts[i];
                ImGui::Checkbox("Enabled", &binding.enabled);
                uint64_t scriptHandle = static_cast<uint64_t>(binding.script);
                if (ImGui::InputScalar("Handle", ImGuiDataType_U64, &scriptHandle)) {
                    binding.script = asset::AssetHandle{scriptHandle};
                }
                acceptAssetHandleDrop(asset::AssetType::Script, binding.script);
                ImGui::PopID();
            }

            if (scriptToDelete >= 0) {
                script.scripts.erase(script.scripts.begin() + scriptToDelete);
            }
        }
    }

    if (m_scene.hasComponent<scene::CameraComponent>(m_selected_entity)) {
        const bool open = ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("CameraPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                m_scene.removeComponent<scene::CameraComponent>(m_selected_entity);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::CameraComponent>(m_selected_entity)) {
            auto& camera = m_scene.getComponent<scene::CameraComponent>(m_selected_entity);
            ImGui::Checkbox("Primary", &camera.primary);
            float exposure = camera.camera.getExposure();
            if (ImGui::DragFloat("Exposure", &exposure, 0.05f, 0.0f, 64.0f, "%.3f")) {
                camera.camera.setExposure(exposure);
            }

            using ProjectionType = renderer::interface::Camera::ProjectionType;
            int projectionType = camera.camera.getProjectionType() == ProjectionType::Orthographic ? 1 : 0;
            const char* projectionTypes[] = {"Perspective", "Orthographic"};
            if (ImGui::Combo("Projection", &projectionType, projectionTypes, 2)) {
                if (projectionType == 1) {
                    camera.camera.setOrthographic(10.0f, 0.1f, 100.0f);
                } else {
                    camera.camera.setPerspective(glm::radians(45.0f), 0.1f, 100.0f);
                }
            }
        }
    }

    if (m_scene.hasComponent<scene::DirectionalLightComponent>(m_selected_entity)) {
        const bool open = ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("DirectionalLightPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                m_scene.removeComponent<scene::DirectionalLightComponent>(m_selected_entity);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::DirectionalLightComponent>(m_selected_entity)) {
            auto& light = m_scene.getComponent<scene::DirectionalLightComponent>(m_selected_entity);
            ImGui::ColorEdit3("Color", &light.color.x);
            ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 1000.0f, "%.3f");

            if (ImGui::TreeNodeEx("Shadow", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enabled", &light.shadow.enabled);

                drawShadowMapSizeControl(light.shadow.map_size);
                light.shadow.map_size = std::max(light.shadow.map_size, 1u);

                ImGui::DragFloat("Max Distance", &light.shadow.max_distance, 0.5f, 0.0f, 10000.0f, "%.2f");
                light.shadow.max_distance = std::max(light.shadow.max_distance, 0.0f);

                ImGui::DragFloat("Bias", &light.shadow.bias, 0.0001f, 0.0f, 0.1f, "%.6f");
                light.shadow.bias = std::max(light.shadow.bias, 0.0f);

                ImGui::DragFloat("Normal Bias", &light.shadow.normal_bias, 0.001f, 0.0f, 1.0f, "%.4f");
                light.shadow.normal_bias = std::max(light.shadow.normal_bias, 0.0f);

                int pcfRadius = static_cast<int>(light.shadow.pcf_radius);
                if (ImGui::SliderInt("PCF Radius", &pcfRadius, 0, 4)) {
                    light.shadow.pcf_radius = static_cast<uint32_t>(std::clamp(pcfRadius, 0, 4));
                }

                int cascadeCount = static_cast<int>(light.shadow.cascade_count);
                if (ImGui::SliderInt("Cascade Count", &cascadeCount, 1, 4)) {
                    light.shadow.cascade_count = static_cast<uint32_t>(std::clamp(cascadeCount, 1, 4));
                }

                ImGui::DragFloat("Cascade Split Lambda", &light.shadow.cascade_split_lambda, 0.01f, 0.0f, 1.0f, "%.3f");
                light.shadow.cascade_split_lambda = std::clamp(light.shadow.cascade_split_lambda, 0.0f, 1.0f);

                ImGui::DragFloat("Cascade Seam Blend", &light.shadow.cascade_seam_blend, 0.1f, 0.0f, 1000.0f, "%.2f");
                light.shadow.cascade_seam_blend = std::max(light.shadow.cascade_seam_blend, 0.0f);

                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}

void InspectorPanel::syncRotationEditor(scene::Entity entity, const glm::quat& rotation)
{
    const auto normalizedRotation = safeNormalize(rotation);
    if (m_rotation_edit_entity.getHandle() == entity.getHandle() &&
        sameOrientation(m_rotation_edit_source, normalizedRotation)) {
        return;
    }

    m_rotation_edit_entity = entity;
    m_rotation_edit_source = normalizedRotation;
    m_rotation_edit_degrees = glm::degrees(glm::eulerAngles(normalizedRotation));
}

} // namespace lunalite::editor
