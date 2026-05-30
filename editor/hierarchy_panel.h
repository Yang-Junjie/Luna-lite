#pragma once

#include "../LunaLite/scene/scene.h"

namespace lunalite::editor {

class HierarchyPanel {
public:
    HierarchyPanel(scene::Scene& scene, scene::Entity& selected_entity);

    void onImGuiRender();

private:
    scene::Scene& m_scene;
    scene::Entity& m_selected_entity;
};

} // namespace lunalite::editor
