#include "debug_panel.h"

#include <algorithm>
#include <imgui.h>

namespace lunalite::editor {

void DebugPanel::onImGuiRender()
{
    ImGui::Begin("Debug");

    ImGui::TextUnformatted("Native Overlay");
    ImGui::Checkbox("Mesh AABB", &m_overlay_settings.mesh_aabb);
    ImGui::Checkbox("Geometry Culling AABB", &m_overlay_settings.geometry_culling_aabb);
    ImGui::Checkbox("Selected AABB", &m_overlay_settings.selected_aabb);
    ImGui::Checkbox("Depth Test Lines", &m_overlay_settings.depth_test);
    ImGui::Checkbox("Show Culling Frustum", &m_overlay_settings.show_culling_frustum);
    if (ImGui::SliderFloat(
            "Frustum Display Depth", &m_overlay_settings.culling_frustum_display_depth, 0.02f, 1.0f, "%.2f")) {
        m_overlay_settings.culling_frustum_display_depth =
            std::clamp(m_overlay_settings.culling_frustum_display_depth, 0.02f, 1.0f);
    }

    const char* cullingSources[] = {
        "Active Editor Camera",
        "Frozen Editor Camera",
    };
    int sourceIndex = static_cast<int>(m_overlay_settings.culling_source);
    if (ImGui::Combo("Culling Source", &sourceIndex, cullingSources, IM_ARRAYSIZE(cullingSources))) {
        m_overlay_settings.culling_source = static_cast<DebugFrustumSource>(sourceIndex);
    }

    if (ImGui::Button("Capture Current Frustum")) {
        m_capture_culling_frustum_requested = true;
    }
    if (m_overlay_settings.culling_source == DebugFrustumSource::FrozenEditorCamera &&
        !m_frozen_culling_frustum_valid) {
        ImGui::TextDisabled("Frozen frustum has not been captured.");
    }

    ImGui::End();
}

bool DebugPanel::consumeCaptureCullingFrustumRequest()
{
    const bool requested = m_capture_culling_frustum_requested;
    m_capture_culling_frustum_requested = false;
    return requested;
}

void DebugPanel::setFrozenCullingFrustumValid(bool valid)
{
    m_frozen_culling_frustum_valid = valid;
}

} // namespace lunalite::editor
