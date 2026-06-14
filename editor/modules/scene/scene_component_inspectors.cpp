#include "../../../LunaLite/scene/component_registry.h"
#include "../../../LunaLite/scene/scene_component_registry.h"
#include "../../../LunaLite/scene/scene_components.h"
#include "../../component_inspector_widgets.h"
#include "../../editor_actions.h"
#include "scene_component_inspectors.h"

#include <cmath>
#include <cstring>

#include <array>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

namespace lunalite::editor {
namespace {
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

struct TransformRotationEditor {
    scene::Entity entity;
    glm::vec3 degrees{0.0f};
    glm::quat source{1.0f, 0.0f, 0.0f, 0.0f};

    void sync(scene::Entity targetEntity, const glm::quat& rotation)
    {
        const auto normalizedRotation = safeNormalize(rotation);
        if (entity.getHandle() == targetEntity.getHandle() && sameOrientation(source, normalizedRotation)) {
            return;
        }

        entity = targetEntity;
        source = normalizedRotation;
        degrees = glm::degrees(glm::eulerAngles(normalizedRotation));
    }
};

TransformRotationEditor& rotationEditor()
{
    static TransformRotationEditor editor;
    return editor;
}

bool drawTagInspector(scene::Scene& scene, scene::Entity selectedEntity)
{
    if (!scene.hasComponent<scene::TagComponent>(selectedEntity)) {
        return false;
    }

    auto& tag = scene.getComponent<scene::TagComponent>(selectedEntity);
    std::array<char, 256> buffer{};
    const size_t copySize = tag.tag.size() < buffer.size() - 1 ? tag.tag.size() : buffer.size() - 1;
    std::memcpy(buffer.data(), tag.tag.data(), copySize);
    return drawLiveSceneEdit(
        scene,
        actions::EditTagCommandId,
        [&]() {
            return ImGui::InputText("Name", buffer.data(), buffer.size());
        },
        [&]() {
            tag.tag = buffer.data();
        });
}

bool drawTransformInspector(scene::Scene& scene, scene::Entity selectedEntity)
{
    if (!scene.hasComponent<scene::TransformComponent>(selectedEntity)) {
        return false;
    }

    bool changed = false;
    auto& transform = scene.getComponent<scene::TransformComponent>(selectedEntity);
    auto translation = transform.translation;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditTransformCommandId,
        [&]() {
            return ImGui::DragFloat3("Translation", &translation.x, 0.1f);
        },
        [&]() {
            transform.translation = translation;
        });

    auto& editor = rotationEditor();
    editor.sync(selectedEntity, transform.rotation);
    auto rotation = editor.degrees;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditTransformCommandId,
        [&]() {
            return ImGui::DragFloat3("Rotation", &rotation.x, 1.0f);
        },
        [&]() {
            const auto delta = rotation - editor.degrees;
            transform.rotation = safeNormalize(transform.rotation * glm::quat{glm::radians(delta)});
            editor.degrees = rotation;
            editor.source = transform.rotation;
        });

    auto scale = transform.scale;
    changed |= drawLiveSceneEdit(
        scene,
        actions::EditTransformCommandId,
        [&]() {
            return ImGui::DragFloat3("Scale", &scale.x, 0.1f);
        },
        [&]() {
            transform.scale = scale;
        });

    return changed;
}
} // namespace

void registerSceneComponentInspectors(scene::ComponentRegistry& registry)
{
    registry.setInspector(scene::TagComponentType, &drawTagInspector);
    registry.setInspector(scene::TransformComponentType, &drawTransformInspector);
}

} // namespace lunalite::editor
