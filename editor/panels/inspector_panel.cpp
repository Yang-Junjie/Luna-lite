#include "../../LunaLite/scene/components.h"
#include "content_browser_panel.h"
#include "inspector_panel.h"

#include <cstdint>
#include <cstring>

#include <array>
#include <imgui.h>

namespace lunalite::editor {
namespace {
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
        if (!m_scene.hasComponent<scene::MeshComponent>(m_selected_entity) && ImGui::MenuItem("Mesh")) {
            m_scene.addComponent<scene::MeshComponent>(m_selected_entity);
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
            auto rotation = glm::eulerAngles(transform.rotation);
            if (ImGui::DragFloat3("Rotation", &rotation.x, 0.1f)) {
                transform.rotation = glm::normalize(glm::quat{rotation});
            }
            ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f);
        }
    }

    if (m_scene.hasComponent<scene::MeshComponent>(m_selected_entity)) {
        const bool open = ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen);
        if (ImGui::BeginPopupContextItem("MeshPopup")) {
            if (ImGui::MenuItem("Delete Component")) {
                m_scene.removeComponent<scene::MeshComponent>(m_selected_entity);
            }
            ImGui::EndPopup();
        }
        if (open && m_scene.hasComponent<scene::MeshComponent>(m_selected_entity)) {
            auto& mesh = m_scene.getComponent<scene::MeshComponent>(m_selected_entity);
            uint64_t meshHandle = static_cast<uint64_t>(mesh.mesh);
            if (ImGui::InputScalar("Handle", ImGuiDataType_U64, &meshHandle)) {
                mesh.mesh = asset::AssetHandle{meshHandle};
            }
            acceptAssetHandleDrop(asset::AssetType::Mesh, mesh.mesh);
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
            ImGui::DragFloat3("Direction", &light.direction.x, 0.1f);
            ImGui::ColorEdit3("Ambient", &light.ambient.x);
            ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
            ImGui::ColorEdit3("Specular", &light.specular.x);
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
