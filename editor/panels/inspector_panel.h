#pragma once

#include "../../LunaLite/scene/scene.h"
#include "../../LunaLiteTooling/context/selection_context.h"

namespace lunalite::editor {

class InspectorPanel {
public:
    InspectorPanel(scene::Scene& scene, tooling::SelectionContext& selection);

    void onImGuiRender();

private:
    scene::Scene& m_scene;
    tooling::SelectionContext& m_selection;
};

} // namespace lunalite::editor
