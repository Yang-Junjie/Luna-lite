#include "../../LunaLite/asset/builtin/builtin_assets.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/scene/components.h"
#include "../../LunaLiteTooling/commands/command_registry.h"
#include "../../LunaLiteTooling/commands/scene_commands.h"
#include "../../LunaLiteTooling/context/tool_context.h"
#include "content_browser_panel.h"
#include "hierarchy_panel.h"

#include <cstdint>

#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace lunalite::editor {
namespace {
inline constexpr const char* EntityDragDropPayloadName = "LUNALITE_ENTITY";

struct EntityDragDropPayload {
    uint32_t handle{0};
};

using EntityUnderlying = std::underlying_type_t<entt::entity>;

uint64_t entityToCommandValue(scene::Entity entity)
{
    return static_cast<uint64_t>(static_cast<EntityUnderlying>(entity.getHandle()));
}

scene::Entity entityFromCommandValue(uint64_t value)
{
    return scene::Entity{static_cast<entt::entity>(static_cast<EntityUnderlying>(value))};
}

tooling::CommandResult
    executeSceneCommand(scene::Scene& scene, std::string_view commandId, const tooling::CommandArgs& args = {})
{
    tooling::ToolContext context;
    context.setScene(scene);
    return tooling::CommandRegistry::get().execute(commandId, context, args);
}

std::optional<scene::Entity>
    createEntityWithCommand(scene::Scene& scene, std::string name = {}, scene::Entity parent = {})
{
    tooling::CommandArgs args;
    if (!name.empty()) {
        args.set("name", std::move(name));
    }
    if (parent) {
        args.set("parent_entity", entityToCommandValue(parent));
    }

    const auto result = executeSceneCommand(scene, tooling::CreateEntityCommandId, args);
    if (!result.success) {
        return std::nullopt;
    }

    const auto* createdEntity = result.get<uint64_t>("created_entity");
    if (createdEntity == nullptr) {
        return std::nullopt;
    }
    return entityFromCommandValue(*createdEntity);
}

bool deleteEntityWithCommand(scene::Scene& scene, scene::Entity entity)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    return executeSceneCommand(scene, tooling::DeleteEntityCommandId, args).success;
}

bool setParentWithCommand(scene::Scene& scene, scene::Entity entity, scene::Entity parent, bool keepWorldTransform)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("parent_entity", entityToCommandValue(parent));
    args.set("keep_world_transform", keepWorldTransform);
    return executeSceneCommand(scene, tooling::SetParentCommandId, args).success;
}

bool clearParentWithCommand(scene::Scene& scene, scene::Entity entity, bool keepWorldTransform)
{
    tooling::CommandArgs args;
    args.set("entity", entityToCommandValue(entity));
    args.set("keep_world_transform", keepWorldTransform);
    return executeSceneCommand(scene, tooling::ClearParentCommandId, args).success;
}

std::optional<scene::Entity> entityFromCommandResult(const tooling::CommandResult& result)
{
    if (const auto* created = result.get<uint64_t>("created_entity")) {
        return entityFromCommandValue(*created);
    }
    if (const auto* affected = result.get<uint64_t>("affected_entity")) {
        return entityFromCommandValue(*affected);
    }
    if (const auto* entity = result.get<uint64_t>("entity")) {
        return entityFromCommandValue(*entity);
    }
    return std::nullopt;
}

std::optional<scene::Entity> createEntityFromAssetWithCommand(scene::Scene& scene,
                                                              const AssetDragDropPayload& payload,
                                                              scene::Entity targetEntity = {})
{
    if (!payload.handle.isValid()) {
        return std::nullopt;
    }

    tooling::CommandArgs args;
    args.set("source_asset", payload.handle);
    args.set("asset_type", static_cast<uint64_t>(payload.type));
    if (targetEntity) {
        args.set("target_entity", entityToCommandValue(targetEntity));
    }

    const auto result = executeSceneCommand(scene, tooling::CreateEntityFromAssetCommandId, args);
    if (!result.success) {
        LUNA_CORE_ERROR("Failed to create entity from asset '{}': {}",
                        payload.handle.toString(),
                        result.message.empty() ? "unknown error" : result.message);
        return std::nullopt;
    }

    return entityFromCommandResult(result);
}

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
                    setParentWithCommand(scene, dragged, targetEntity, true)) {
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
                if (auto entity = createEntityFromAssetWithCommand(scene, assetPayload, targetEntity)) {
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
            if (auto child = createEntityWithCommand(scene, {}, entity)) {
                selection.selectEntity(*child);
            }
        }
        if (scene.getParent(entity) && ImGui::MenuItem("Unparent")) {
            clearParentWithCommand(scene, entity, true);
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
                        if (clearParentWithCommand(m_scene, dragged, true)) {
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
                if (auto entity = createEntityWithCommand(m_scene)) {
                    m_selection.selectEntity(*entity);
                }
            }
            if (ImGui::BeginMenu("Create Builtin Mesh")) {
                if (ImGui::MenuItem("Cube")) {
                    if (auto entity = createEntityWithCommand(m_scene, "Cube")) {
                        auto& meshRenderer = m_scene.addComponent<scene::MeshRendererComponent>(*entity);
                        meshRenderer.mesh = asset::builtin::cubeMeshHandle();
                        meshRenderer.materials = {asset::builtin::defaultMaterialHandle()};
                        m_selection.selectEntity(*entity);
                    }
                }
                if (ImGui::MenuItem("Plane")) {
                    if (auto entity = createEntityWithCommand(m_scene, "Plane")) {
                        auto& meshRenderer = m_scene.addComponent<scene::MeshRendererComponent>(*entity);
                        meshRenderer.mesh = asset::builtin::planeMeshHandle();
                        meshRenderer.materials = {asset::builtin::defaultMaterialHandle()};
                        m_selection.selectEntity(*entity);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
    }

    if (entity_to_delete) {
        deleteEntityWithCommand(m_scene, entity_to_delete);
        if (m_selection.isEntity() && !m_scene.isValidEntity(m_selection.selectedEntity())) {
            m_selection.clear();
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
