#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/builtin/builtin_assets.h"
#include "../../LunaLite/scene/components.h"
#include "content_browser_panel.h"
#include "hierarchy_panel.h"

#include <cstdint>

#include <imgui.h>
#include <string>

namespace lunalite::editor {
namespace {
inline constexpr const char* EntityDragDropPayloadName = "LUNALITE_ENTITY";

struct EntityDragDropPayload {
    uint32_t handle{0};
};

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

void createEntityWithModel(scene::Scene& scene,
                           scene::Entity& selectedEntity,
                           asset::AssetHandle modelHandle,
                           const std::string& tag,
                           scene::Entity parent = {})
{
    auto entity = scene.createEntity();
    auto& model = scene.addComponent<scene::ModelComponent>(entity);
    model.model = modelHandle;
    if (parent) {
        scene.setParent(entity, parent, false);
    }
    auto& tagComponent = scene.getComponent<scene::TagComponent>(entity);
    tagComponent.tag = tag;
    selectedEntity = entity;
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
        if (scene.isValidEntity(targetEntity)) {
            scene.setParent(entity, targetEntity, false);
        }
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

bool acceptEntityDrop(scene::Scene& scene, scene::Entity& selectedEntity, scene::Entity targetEntity)
{
    bool accepted = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityDragDropPayloadName)) {
            if (payload->DataSize == sizeof(EntityDragDropPayload)) {
                const auto& entityPayload = *static_cast<const EntityDragDropPayload*>(payload->Data);
                const scene::Entity dragged{static_cast<entt::entity>(entityPayload.handle)};
                if (scene.isValidEntity(dragged) && (!targetEntity || dragged.getHandle() != targetEntity.getHandle()) &&
                    scene.setParent(dragged, targetEntity, true)) {
                    selectedEntity = dragged;
                    accepted = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
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

void drawEntityNode(scene::Scene& scene,
                    scene::Entity& selectedEntity,
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

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selectedEntity.getHandle() == handle) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(static_cast<int>(entityId));
    const bool open = ImGui::TreeNodeEx("##entity", flags, "%s", label.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedEntity = entity;
    }

    if (ImGui::BeginDragDropSource()) {
        const EntityDragDropPayload payload{.handle = entityId};
        ImGui::SetDragDropPayload(EntityDragDropPayloadName, &payload, sizeof(payload));
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndDragDropSource();
    }

    acceptEntityDrop(scene, selectedEntity, entity);
    acceptAssetDrop(scene, selectedEntity, entity);

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Child")) {
            auto child = scene.createEntity();
            scene.setParent(child, entity, false);
            selectedEntity = child;
        }
        if (scene.getParent(entity) && ImGui::MenuItem("Unparent")) {
            scene.clearParent(entity, true);
        }
        if (ImGui::MenuItem("Delete")) {
            entityToDelete = entity;
        }
        ImGui::EndPopup();
    }

    if (open && !children.empty()) {
        for (const auto child : children) {
            drawEntityNode(scene, selectedEntity, child, entityToDelete);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
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
    for (const auto entity : m_scene.getRootEntities()) {
        drawEntityNode(m_scene, m_selected_entity, entity, entity_to_delete);
    }

    const auto available = ImGui::GetContentRegionAvail();
    if (available.y > 0.0f) {
        ImGui::Dummy(available);
        acceptAssetDrop(m_scene, m_selected_entity);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EntityDragDropPayloadName)) {
                if (payload->DataSize == sizeof(EntityDragDropPayload)) {
                    const auto& entityPayload = *static_cast<const EntityDragDropPayload*>(payload->Data);
                    const scene::Entity dragged{static_cast<entt::entity>(entityPayload.handle)};
                    if (m_scene.isValidEntity(dragged)) {
                        m_scene.clearParent(dragged, true);
                        m_selected_entity = dragged;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextWindow("HierarchyEmptyContext",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Entity")) {
                m_selected_entity = m_scene.createEntity();
            }
            if (ImGui::BeginMenu("Create Builtin Mesh")) {
                if (ImGui::MenuItem("Cube")) {
                    createEntityWithModel(m_scene, m_selected_entity, asset::builtin::cubeModelHandle(), "Cube");
                }
                if (ImGui::MenuItem("Plane")) {
                    createEntityWithModel(m_scene, m_selected_entity, asset::builtin::planeModelHandle(), "Plane");
                }
                ImGui::EndMenu();
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
