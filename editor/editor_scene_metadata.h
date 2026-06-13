#pragma once

#include "editor_camera.h"

#include <filesystem>

namespace lunalite::editor {

bool loadEditorSceneCamera(const std::filesystem::path& scene_path, EditorCamera& editor_camera);
bool saveEditorSceneCamera(const std::filesystem::path& scene_path, const EditorCamera& editor_camera);

} // namespace lunalite::editor
