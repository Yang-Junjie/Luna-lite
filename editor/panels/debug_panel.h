#pragma once

namespace lunalite::editor {

enum class DebugFrustumSource {
    ActiveEditorCamera,
    FrozenEditorCamera
};

struct DebugOverlaySettings {
    bool mesh_aabb{false};
    bool geometry_culling_aabb{false};
    bool selected_aabb{true};
    bool depth_test{true};
    bool show_culling_frustum{false};
    float culling_frustum_display_depth{0.25f};
    DebugFrustumSource culling_source{DebugFrustumSource::ActiveEditorCamera};
};

class DebugPanel {
public:
    void onImGuiRender();
    bool consumeCaptureCullingFrustumRequest();
    void setFrozenCullingFrustumValid(bool valid);

    const DebugOverlaySettings& overlaySettings() const
    {
        return m_overlay_settings;
    }

private:
    DebugOverlaySettings m_overlay_settings;
    bool m_capture_culling_frustum_requested{false};
    bool m_frozen_culling_frustum_valid{false};
};

} // namespace lunalite::editor
