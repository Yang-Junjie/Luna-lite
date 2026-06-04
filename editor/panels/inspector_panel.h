#pragma once

#include "../../LunaLite/scene/scene.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lunalite::editor {

class InspectorPanel {
public:
    InspectorPanel(scene::Scene& scene, scene::Entity& selected_entity);

    void onImGuiRender();

private:
    void syncRotationEditor(scene::Entity entity, const glm::quat& rotation);

    scene::Scene& m_scene;
    scene::Entity& m_selected_entity;
    scene::Entity m_rotation_edit_entity;
    glm::vec3 m_rotation_edit_degrees{0.0f};
    glm::quat m_rotation_edit_source{1.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace lunalite::editor
