#pragma once

#include <cstdint>

#include <algorithm>

namespace lunalite::diagnostics {

struct FrameStats {
    uint64_t frame_index{0};
    float delta_seconds{0.0f};
    float frame_time_ms{0.0f};
    float fps{0.0f};
    float average_frame_time_ms{0.0f};
    float average_fps{0.0f};
};

struct RenderStats {
    uint32_t mesh_commands{0};
    uint32_t debug_lines{0};

    uint32_t draw_calls_total{0};
    uint32_t geometry_draw_calls{0};
    uint32_t shadow_draw_calls{0};
    uint32_t debug_line_draw_calls{0};
    uint32_t lighting_draw_calls{0};
    uint32_t skybox_draw_calls{0};
};

struct ImGuiStats {
    uint32_t draw_calls{0};
    uint32_t vertices{0};
    uint32_t indices{0};
};

class RuntimeStats {
public:
    const FrameStats& frame() const
    {
        return m_frame;
    }

    const RenderStats& render() const
    {
        return m_render;
    }

    const ImGuiStats& imgui() const
    {
        return m_imgui;
    }

    void updateFrame(float delta_seconds)
    {
        m_frame.frame_index += 1;
        m_frame.delta_seconds = std::max(delta_seconds, 0.0f);
        m_frame.frame_time_ms = m_frame.delta_seconds * 1000.0f;
        m_frame.fps = m_frame.delta_seconds > 0.0f ? 1.0f / m_frame.delta_seconds : 0.0f;

        if (m_frame.frame_index == 1) {
            m_frame.average_frame_time_ms = m_frame.frame_time_ms;
        } else {
            constexpr float smoothing = 0.1f;
            m_frame.average_frame_time_ms =
                m_frame.average_frame_time_ms * (1.0f - smoothing) + m_frame.frame_time_ms * smoothing;
        }
        m_frame.average_fps = m_frame.average_frame_time_ms > 0.0f ? 1000.0f / m_frame.average_frame_time_ms : 0.0f;
    }

    void setRenderStats(const RenderStats& stats)
    {
        m_render = stats;
    }

    void setImGuiStats(const ImGuiStats& stats)
    {
        m_imgui = stats;
    }

    void clearImGuiStats()
    {
        m_imgui = ImGuiStats{};
    }

private:
    FrameStats m_frame;
    RenderStats m_render;
    ImGuiStats m_imgui;
};

} // namespace lunalite::diagnostics
