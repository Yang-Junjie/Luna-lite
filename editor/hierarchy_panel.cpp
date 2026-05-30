#include "hierarchy_panel.h"

#include <cstdint>

#include <imgui.h>
#include <string>

namespace lunalite::editor {

HierarchyPanel::HierarchyPanel(scene::Scene& scene, scene::Entity& selected_entity)
    : m_scene(scene),
      m_selected_entity(selected_entity)
{}

void HierarchyPanel::onImGuiRender()
{
    ImGui::Begin("Hierarchy");

    if (ImGui::Button("Add Entity")) {
        m_selected_entity = m_scene.createEntity();
    }

    ImGui::SameLine();

    const bool can_delete_selected = m_scene.isValidEntity(m_selected_entity);
    if (!can_delete_selected) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Delete Selected")) {
        m_scene.destroyEntity(m_selected_entity);
        m_selected_entity = scene::Entity{};
    }

    if (!can_delete_selected) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    scene::Entity entity_to_delete;
    for (const auto entity : m_scene.getEntities()) {
        const auto handle = entity.getHandle();
        const auto entity_id = static_cast<uint32_t>(handle);
        const std::string label = "Entity " + std::to_string(entity_id);
        const bool selected = m_selected_entity.getHandle() == handle;

        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selected_entity = scene::Entity{handle};
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                entity_to_delete = scene::Entity{handle};
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
