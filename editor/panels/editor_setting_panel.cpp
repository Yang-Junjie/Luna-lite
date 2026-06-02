#include "../../LunaLite/core/log.h"
#include "editor_setting_panel.h"

#include <fstream>
#include <imgui.h>
#include <system_error>
#include <yaml-cpp/yaml.h>

namespace lunalite::editor {
namespace {
std::filesystem::path editorConfigPath()
{
    return std::filesystem::path{"settings"} / "editor" / "editor_config.yaml";
}
} // namespace

EditorSettingPanel::EditorSettingPanel(EditorCamera& editor_camera)
    : m_editor_camera(editor_camera),
      m_config_path(editorConfigPath())
{
    load();
}

void EditorSettingPanel::onImGuiRender()
{
    ImGui::Begin("Editor Settings");

    const auto configPathText = m_config_path.string();
    ImGui::TextDisabled("%s", configPathText.c_str());
    ImGui::Separator();

    ImGui::TextUnformatted("Editor Camera");
    float exposure = m_editor_camera.getExposure();
    if (ImGui::DragFloat("Exposure", &exposure, 0.05f, 0.0f, 64.0f, "%.3f")) {
        m_editor_camera.setExposure(exposure);
        save();
    }

    ImGui::End();
}

void EditorSettingPanel::load()
{
    if (!std::filesystem::exists(m_config_path)) {
        save();
        return;
    }

    try {
        const auto root = YAML::LoadFile(m_config_path.string());
        if (const auto editorCamera = root["EditorCamera"]) {
            m_editor_camera.setExposure(editorCamera["Exposure"].as<float>(m_editor_camera.getExposure()));
        }
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load editor settings '{}': {}", m_config_path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load editor settings '{}': {}", m_config_path.string(), exception.what());
    }
}

void EditorSettingPanel::save() const
{
    std::error_code error;
    std::filesystem::create_directories(m_config_path.parent_path(), error);
    if (error) {
        LUNA_CORE_ERROR("Failed to create editor settings directory '{}': {}",
                        m_config_path.parent_path().string(),
                        error.message());
        return;
    }

    YAML::Node root;
    root["EditorCamera"]["Exposure"] = m_editor_camera.getExposure();

    std::ofstream out(m_config_path);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open editor settings file for writing: '{}'", m_config_path.string());
        return;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write editor settings file: '{}'", m_config_path.string());
    }
}

} // namespace lunalite::editor
