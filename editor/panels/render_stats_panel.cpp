#include "../../LunaLite/core/application.h"
#include "render_stats_panel.h"

#include <imgui.h>

namespace lunalite::editor {

void RenderStatsPanel::onImGuiRender()
{
    const auto& stats = core::Application::get().getStats();
    const auto& frame = stats.frame();
    const auto& render = stats.render();
    const auto& imgui = stats.imgui();

    ImGui::Begin("Render Stats");

    ImGui::Text("Frame %llu", static_cast<unsigned long long>(frame.frame_index));
    ImGui::Text("Frame Time %.3f ms", frame.frame_time_ms);
    ImGui::Text("FPS %.1f", frame.fps);
    ImGui::Text("Average %.3f ms / %.1f FPS", frame.average_frame_time_ms, frame.average_fps);

    ImGui::Separator();
    ImGui::TextUnformatted("Scene");
    ImGui::Text("Mesh Commands %u", render.mesh_commands);
    ImGui::Text("Debug Lines %u", render.debug_lines);

    ImGui::Separator();
    ImGui::TextUnformatted("Draw Calls");
    ImGui::Text("Total %u", render.draw_calls_total);
    ImGui::Text("Geometry %u", render.geometry_draw_calls);
    ImGui::Text("Shadow %u", render.shadow_draw_calls);
    ImGui::Text("Debug Lines %u", render.debug_line_draw_calls);
    ImGui::Text("Lighting %u", render.lighting_draw_calls);
    ImGui::Text("Skybox %u", render.skybox_draw_calls);

    ImGui::Separator();
    ImGui::TextUnformatted("ImGui");
    ImGui::Text("Draw Calls %u", imgui.draw_calls);
    ImGui::Text("Vertices %u", imgui.vertices);
    ImGui::Text("Indices %u", imgui.indices);

    ImGui::End();
}

} // namespace lunalite::editor
