#pragma once

#include "../../LunaLite/scene/scene.h"

namespace lunalite::editor {

class ScenePanel {
public:
    explicit ScenePanel(scene::Scene& scene);

    void onImGuiRender();

private:
    scene::Scene& m_scene;
};

} // namespace lunalite::editor
