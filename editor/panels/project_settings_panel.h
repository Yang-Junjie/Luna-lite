#pragma once

#include "../../LunaLite/project/project_info.h"

#include <filesystem>
#include <optional>

namespace lunalite::editor {

class ProjectSettingsPanel {
public:
    explicit ProjectSettingsPanel(const std::filesystem::path& currentScenePath);

    void onImGuiRender();

private:
    void syncFromProject();
    void markDirty();
    void save();
    void reset();
    void chooseStartScene();
    void setStartSceneToCurrentScene();

    const std::filesystem::path& m_current_scene_path;
    project::ProjectInfo m_edit_info;
    std::optional<std::filesystem::path> m_loaded_project_root;
    bool m_loaded{false};
    bool m_dirty{false};
};

} // namespace lunalite::editor
