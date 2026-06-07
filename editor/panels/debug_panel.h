#pragma once

namespace lunalite::editor {

struct DebugOverlaySettings {
    bool mesh_aabb{false};
    bool selected_aabb{true};
    bool depth_test{true};
};

class DebugPanel {
public:
    void onImGuiRender();

    const DebugOverlaySettings& overlaySettings() const
    {
        return m_overlay_settings;
    }

private:
    DebugOverlaySettings m_overlay_settings;
};

} // namespace lunalite::editor
