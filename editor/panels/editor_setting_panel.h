#pragma once

#include "../editor_camera.h"

#include <filesystem>

namespace lunalite::editor {

class EditorSettingPanel {
public:
    explicit EditorSettingPanel(EditorCamera& editor_camera);

    void onImGuiRender();

private:
    void load();
    void save() const;

    EditorCamera& m_editor_camera;
    std::filesystem::path m_config_path;
};

} // namespace lunalite::editor
