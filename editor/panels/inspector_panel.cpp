#include "../../LunaLite/scene/component_registry.h"
#include "../editor_actions.h"
#include "../editor_component_inspectors.h"
#include "inspector_panel.h"

#include <cstdint>

#include <imgui.h>
#include <string>

namespace lunalite::editor {
namespace {
bool drawRegisteredComponentInspector(scene::Scene& scene,
                                      scene::Entity entity,
                                      const scene::ComponentDescriptor& descriptor)
{
    if (descriptor.has == nullptr || descriptor.inspect == nullptr || !descriptor.has(scene, entity)) {
        return false;
    }

    const auto label = std::string{descriptor.display_name};
    const bool open = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    const auto popupId = std::string{descriptor.id} + "Popup";
    if (ImGui::BeginPopupContextItem(popupId.c_str())) {
        if (descriptor.remove != nullptr && ImGui::MenuItem("Delete Component")) {
            actions::removeComponent(scene, entity, descriptor.id);
        }
        ImGui::EndPopup();
    }

    if (open && descriptor.has(scene, entity)) {
        ImGui::PushID(std::string{descriptor.id}.c_str());
        descriptor.inspect(scene, entity);
        ImGui::PopID();
    }

    return true;
}

void registerInspectorCallbacks()
{
    static const bool registered = []() {
        registerEditorComponentInspectors(scene::ComponentRegistry::get());
        return true;
    }();
    (void) registered;
}
} // namespace

InspectorPanel::InspectorPanel(scene::Scene& scene, tooling::SelectionContext& selection)
    : m_scene(scene),
      m_selection(selection)
{
    registerInspectorCallbacks();
}

void InspectorPanel::onImGuiRender()
{
    ImGui::Begin("Inspector");

    if (!m_selection.isEntity() || !m_scene.isValidEntity(m_selection.selectedEntity())) {
        ImGui::TextUnformatted("No entity selected");
        ImGui::End();
        return;
    }

    const auto selectedEntity = m_selection.selectedEntity();

    ImGui::Text("Entity %u", static_cast<uint32_t>(selectedEntity.getHandle()));
    ImGui::Separator();

    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponent");
    }

    if (ImGui::BeginPopup("AddComponent")) {
        for (const auto& descriptor : scene::ComponentRegistry::get().components()) {
            if (!descriptor.user_addable || descriptor.add == nullptr || descriptor.has == nullptr ||
                descriptor.has(m_scene, selectedEntity)) {
                continue;
            }

            const auto label = std::string{descriptor.display_name};
            if (ImGui::MenuItem(label.c_str())) {
                actions::addComponent(m_scene, selectedEntity, descriptor.id);
            }
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    for (const auto& descriptor : scene::ComponentRegistry::get().components()) {
        drawRegisteredComponentInspector(m_scene, selectedEntity, descriptor);
    }

    ImGui::End();
}

} // namespace lunalite::editor
