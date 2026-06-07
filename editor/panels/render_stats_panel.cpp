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
    ImGui::Text("Geometry Visible %u", render.geometry_visible_meshes);
    ImGui::Text("Geometry Culled %u", render.geometry_culled_meshes);
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
    ImGui::TextUnformatted("GPU");
    if (!render.gpu_profiler.supported) {
        ImGui::TextDisabled("Timestamp unavailable");
    } else if (!render.gpu_profiler.valid) {
        ImGui::TextDisabled("Profiler warming up");
    } else {
        ImGui::Text("Frame %.3f ms", render.gpu_profiler.frame_ms);
        ImGui::Text("Shadow %.3f ms", render.gpu_profiler.shadow_ms);
        for (uint32_t cascadeIndex = 0; cascadeIndex < render.gpu_profiler.shadow_cascade_ms.size(); ++cascadeIndex) {
            if (cascadeIndex >= render.shadow_cascade_count) {
                continue;
            }
            ImGui::Text("  Cascade %u %.3f ms", cascadeIndex, render.gpu_profiler.shadow_cascade_ms[cascadeIndex]);
        }
        ImGui::Text("Geometry %.3f ms", render.gpu_profiler.geometry_ms);
        ImGui::Text("Lighting %.3f ms", render.gpu_profiler.lighting_ms);
        ImGui::Text("Skybox %.3f ms", render.gpu_profiler.skybox_ms);
        ImGui::Text("Debug Lines %.3f ms", render.gpu_profiler.debug_lines_ms);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Shadow");
    ImGui::Text("Enabled %s", render.shadow_enabled ? "true" : "false");
    ImGui::Text("Map Size %u", render.shadow_map_size);
    ImGui::Text("Cascade Count %u", render.shadow_cascade_count);
    ImGui::Text("Max Distance %.2f", render.shadow_max_distance);
    ImGui::Text("Bias %.6f / Normal %.4f", render.shadow_bias, render.shadow_normal_bias);
    ImGui::Text("PCF Radius %u", render.shadow_pcf_radius);
    ImGui::Text("Split Lambda %.3f", render.shadow_cascade_split_lambda);
    ImGui::Text("Seam Blend %.2f", render.shadow_cascade_seam_blend);
    ImGui::Text("Caster Padding %.2f", render.shadow_cascade_caster_depth_padding);

    if (ImGui::TreeNodeEx("Cascade Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (uint32_t cascadeIndex = 0; cascadeIndex < render.shadow_cascades.size(); ++cascadeIndex) {
            const auto& cascade = render.shadow_cascades[cascadeIndex];
            if (!cascade.active) {
                ImGui::Text("Cascade %u: inactive", cascadeIndex);
                continue;
            }

            if (render.gpu_profiler.valid) {
                ImGui::Text("Cascade %u: split %.2f, casters %u, draws %u, gpu %.3f ms",
                            cascadeIndex,
                            cascade.split_depth,
                            cascade.caster_meshes,
                            cascade.draw_calls,
                            render.gpu_profiler.shadow_cascade_ms[cascadeIndex]);
            } else {
                ImGui::Text("Cascade %u: split %.2f, casters %u, draws %u",
                            cascadeIndex,
                            cascade.split_depth,
                            cascade.caster_meshes,
                            cascade.draw_calls);
            }
        }
        ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("ImGui");
    ImGui::Text("Draw Calls %u", imgui.draw_calls);
    ImGui::Text("Vertices %u", imgui.vertices);
    ImGui::Text("Indices %u", imgui.indices);

    ImGui::End();
}

} // namespace lunalite::editor
