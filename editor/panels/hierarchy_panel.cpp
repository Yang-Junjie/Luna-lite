#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/scene/components.h"
#include "content_browser_panel.h"
#include "hierarchy_panel.h"

#include <cstdint>

#include <imgui.h>
#include <string>

namespace lunalite::editor {
namespace {
void setEntityTagFromAsset(scene::Scene& scene, scene::Entity entity, asset::AssetHandle handle)
{
    if (const auto* metadata = asset::AssetManager::get().getMetadata(handle)) {
        auto& tag = scene.getComponent<scene::TagComponent>(entity);
        tag.tag = metadata->Name.empty() ? metadata->FilePath.stem().string() : metadata->Name;
    }
}

void addScriptToEntity(scene::Scene& scene, scene::Entity entity, asset::AssetHandle handle)
{
    if (!scene.hasComponent<scene::ScriptComponent>(entity)) {
        scene.addComponent<scene::ScriptComponent>(entity);
    }

    auto& script = scene.getComponent<scene::ScriptComponent>(entity);
    script.scripts.push_back({handle, true});
}

void createEntityFromAsset(scene::Scene& scene,
                           scene::Entity& selectedEntity,
                           const AssetDragDropPayload& payload,
                           scene::Entity targetEntity = {})
{
    const auto handle = payload.handle;
    if (!handle.isValid()) {
        return;
    }

    if (payload.type == asset::AssetType::Model) {
        auto entity = scene.createEntity();
        auto& model = scene.addComponent<scene::ModelComponent>(entity);
        model.model = handle;
        setEntityTagFromAsset(scene, entity, handle);
        selectedEntity = entity;
        return;
    }

    if (payload.type == asset::AssetType::Script) {
        const bool useTargetEntity = scene.isValidEntity(targetEntity);
        auto entity = useTargetEntity ? targetEntity : scene.createEntity();
        addScriptToEntity(scene, entity, handle);
        if (!useTargetEntity) {
            setEntityTagFromAsset(scene, entity, handle);
        }
        selectedEntity = entity;
    }
}

void acceptAssetDrop(scene::Scene& scene, scene::Entity& selectedEntity, scene::Entity targetEntity = {})
{
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadName)) {
            if (payload->DataSize == sizeof(AssetDragDropPayload)) {
                createEntityFromAsset(
                    scene, selectedEntity, *static_cast<const AssetDragDropPayload*>(payload->Data), targetEntity);
            }
        }
        ImGui::EndDragDropTarget();
    }
}
} // namespace

HierarchyPanel::HierarchyPanel(scene::Scene& scene, scene::Entity& selected_entity)
    : m_scene(scene),
      m_selected_entity(selected_entity)
{}

void HierarchyPanel::onImGuiRender()
{
    ImGui::Begin("Hierarchy");

    scene::Entity entity_to_delete;
    for (const auto entity : m_scene.getEntities()) {
        const auto handle = entity.getHandle();
        const auto entity_id = static_cast<uint32_t>(handle);
        std::string label = "Entity " + std::to_string(entity_id);
        if (m_scene.hasComponent<scene::TagComponent>(entity)) {
            const auto& tag = m_scene.getComponent<scene::TagComponent>(entity);
            label = (tag.tag.empty() ? "empty tag" : tag.tag) + "##" + std::to_string(entity_id);
        }
        const bool selected = m_selected_entity.getHandle() == handle;

        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selected_entity = scene::Entity{handle};
        }
        acceptAssetDrop(m_scene, m_selected_entity, scene::Entity{handle});

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                entity_to_delete = scene::Entity{handle};
            }
            ImGui::EndPopup();
        }
    }

    const auto available = ImGui::GetContentRegionAvail();
    if (available.y > 0.0f) {
        ImGui::Dummy(available);
        acceptAssetDrop(m_scene, m_selected_entity);

        if (ImGui::BeginPopupContextItem("HierarchyEmptyContext")) {
            if (ImGui::MenuItem("Create Entity")) {
                m_selected_entity = m_scene.createEntity();
            }
            ImGui::EndPopup();
        }
    }

    if (entity_to_delete) {
        m_scene.destroyEntity(entity_to_delete);
        if (m_selected_entity.getHandle() == entity_to_delete.getHandle()) {
            m_selected_entity = scene::Entity{};
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
