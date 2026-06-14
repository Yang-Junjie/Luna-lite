#include "../../LunaLite/asset/builtin/builtin_assets.h"
#include "../../LunaLite/scene/scene_components.h"
#include "../editor_actions.h"
#include "content_browser_panel.h"
#include "hierarchy_panel.h"

#include <imgui.h>
#include <string>

namespace lunalite::editor {
namespace {
inline constexpr const char* EntityDragDropPayloadName = "LUNALITE_ENTITY";

struct EntityDragDropPayload {
    uint32_t handle{0};
};

bool acceptEntityDrop(scene::Scene& scene, tooling::SelectionContext& selection, scene::Entity targetEntity)
{
    bool accepted = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityDragDropPayloadName)) {
            if (payload->DataSize == sizeof(EntityDragDropPayload)) {
                const auto& entityPayload = *static_cast<const EntityDragDropPayload*>(payload->Data);
                const scene::Entity dragged{static_cast<entt::entity>(entityPayload.handle)};
                if (scene.isValidEntity(dragged) &&
                    (!targetEntity || dragged.getHandle() != targetEntity.getHandle()) &&
                    actions::setParent(scene, dragged, targetEntity, true)) {
                    selection.selectEntity(dragged);
                    accepted = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
}

void acceptAssetDrop(scene::Scene& scene, tooling::SelectionContext& selection, scene::Entity targetEntity = {})
{
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadName)) {
            if (payload->DataSize == sizeof(AssetDragDropPayload)) {
                const auto& assetPayload = *static_cast<const AssetDragDropPayload*>(payload->Data);
                if (auto entity =
                        actions::createEntityFromAsset(scene, assetPayload.handle, assetPayload.type, targetEntity)) {
                    selection.selectEntity(*entity);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void drawEntityNode(scene::Scene& scene,
                    tooling::SelectionContext& selection,
                    scene::Entity entity,
                    scene::Entity& entityToDelete)
{
    const auto handle = entity.getHandle();
    const auto entityId = static_cast<uint32_t>(handle);
    const auto children = scene.getChildren(entity);

    std::string label;
    if (scene.hasComponent<scene::TagComponent>(entity)) {
        const auto& tag = scene.getComponent<scene::TagComponent>(entity);
        label = tag.tag.empty() ? "Entity" : tag.tag;
    } else {
        label = "Entity";
    }
    label += "##" + std::to_string(entityId);

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selection.isEntity() && selection.selectedEntity().getHandle() == handle) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(static_cast<int>(entityId));
    const bool open = ImGui::TreeNodeEx("##entity", flags, "%s", label.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selection.selectEntity(entity);
    }

    if (ImGui::BeginDragDropSource()) {
        const EntityDragDropPayload payload{.handle = entityId};
        ImGui::SetDragDropPayload(EntityDragDropPayloadName, &payload, sizeof(payload));
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndDragDropSource();
    }

    acceptEntityDrop(scene, selection, entity);
    acceptAssetDrop(scene, selection, entity);

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Child")) {
            if (auto child = actions::createEntity(scene, {}, entity)) {
                selection.selectEntity(*child);
            }
        }
        if (scene.getParent(entity) && ImGui::MenuItem("Unparent")) {
            actions::clearParent(scene, entity, true);
        }
        if (ImGui::MenuItem("Delete")) {
            entityToDelete = entity;
        }
        ImGui::EndPopup();
    }

    if (open && !children.empty()) {
        for (const auto child : children) {
            drawEntityNode(scene, selection, child, entityToDelete);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}
} // namespace

HierarchyPanel::HierarchyPanel(scene::Scene& scene, tooling::SelectionContext& selection)
    : m_scene(scene),
      m_selection(selection)
{}

void HierarchyPanel::onImGuiRender()
{
    ImGui::Begin("Hierarchy");

    scene::Entity entity_to_delete;
    for (const auto entity : m_scene.getRootEntities()) {
        drawEntityNode(m_scene, m_selection, entity, entity_to_delete);
    }

    const auto available = ImGui::GetContentRegionAvail();
    if (available.y > 0.0f) {
        ImGui::Dummy(available);
        acceptAssetDrop(m_scene, m_selection);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityDragDropPayloadName)) {
                if (payload->DataSize == sizeof(EntityDragDropPayload)) {
                    const auto& entityPayload = *static_cast<const EntityDragDropPayload*>(payload->Data);
                    const scene::Entity dragged{static_cast<entt::entity>(entityPayload.handle)};
                    if (m_scene.isValidEntity(dragged)) {
                        if (actions::clearParent(m_scene, dragged, true)) {
                            m_selection.selectEntity(dragged);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextWindow("HierarchyEmptyContext",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Entity")) {
                if (auto entity = actions::createEntity(m_scene)) {
                    m_selection.selectEntity(*entity);
                }
            }
            if (ImGui::BeginMenu("Create Builtin Mesh")) {
                if (ImGui::MenuItem("Cube")) {
                    if (auto entity = actions::createBuiltinMeshEntity(m_scene,
                                                                       "Cube",
                                                                       asset::builtin::cubeMeshHandle(),
                                                                       asset::builtin::defaultMaterialHandle())) {
                        m_selection.selectEntity(*entity);
                    }
                }
                if (ImGui::MenuItem("Plane")) {
                    if (auto entity = actions::createBuiltinMeshEntity(m_scene,
                                                                       "Plane",
                                                                       asset::builtin::planeMeshHandle(),
                                                                       asset::builtin::defaultMaterialHandle())) {
                        m_selection.selectEntity(*entity);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
    }

    if (entity_to_delete) {
        actions::deleteEntity(m_scene, entity_to_delete);
        if (m_selection.isEntity() && !m_scene.isValidEntity(m_selection.selectedEntity())) {
            m_selection.clear();
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
