#include "debug_panel.h"

#include <imgui.h>

namespace lunalite::editor {

void DebugPanel::onImGuiRender()
{
    ImGui::Begin("Debug");

    ImGui::TextUnformatted("Native Overlay");
    ImGui::Checkbox("Mesh AABB", &m_overlay_settings.mesh_aabb);
    ImGui::Checkbox("Selected AABB", &m_overlay_settings.selected_aabb);
    ImGui::Checkbox("Depth Test Lines", &m_overlay_settings.depth_test);

    ImGui::End();
}

} // namespace lunalite::editor
