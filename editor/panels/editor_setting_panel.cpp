#include "../../LunaLite/core/log.h"
#include "editor_setting_panel.h"

#include <fstream>
#include <imgui.h>
#include <system_error>
#include <yaml-cpp/yaml.h>

namespace lunalite::editor {
std::filesystem::path editorConfigPath()
{
    return std::filesystem::path{"settings"} / "editor" / "editor_config.yaml";
}

EditorSettingPanel::EditorSettingPanel(EditorCamera& editor_camera)
    : m_editor_camera(editor_camera), m_config_path(editorConfigPath())
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
    bool orthographic = m_editor_camera.getProjectionType() == EditorCamera::ProjectionType::Orthographic;
    int projectionType = orthographic ? 1 : 0;
    const char* projectionTypes[] = {"Perspective", "Orthographic"};
    if (ImGui::Combo("Projection", &projectionType, projectionTypes, IM_ARRAYSIZE(projectionTypes))) {
        m_editor_camera.setProjectionType(projectionType == 1 ? EditorCamera::ProjectionType::Orthographic
                                                              : EditorCamera::ProjectionType::Perspective);
        orthographic = m_editor_camera.getProjectionType() == EditorCamera::ProjectionType::Orthographic;
    }

    glm::vec3 position = m_editor_camera.getPosition();
    if (orthographic) {
        if (ImGui::DragFloat2("Position", &position.x, 0.1f)) {
            position.z = m_editor_camera.getPosition().z;
            m_editor_camera.setPosition(position);
        }
        ImGui::BeginDisabled();
        ImGui::DragFloat("Locked Z", &position.z, 0.1f);
        ImGui::EndDisabled();
    } else if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
        m_editor_camera.setPosition(position);
    }

    float yaw = m_editor_camera.getYaw();
    if (orthographic) {
        ImGui::BeginDisabled();
    }
    if (ImGui::DragFloat("Yaw", &yaw, 0.1f)) {
        m_editor_camera.setYaw(yaw);
    }
    if (orthographic) {
        ImGui::EndDisabled();
    }

    float pitch = m_editor_camera.getPitch();
    if (orthographic) {
        ImGui::BeginDisabled();
    }
    if (ImGui::DragFloat("Pitch", &pitch, 0.1f, -89.0f, 89.0f)) {
        m_editor_camera.setPitch(pitch);
    }
    if (orthographic) {
        ImGui::EndDisabled();
    }

    float exposure = m_editor_camera.getExposure();
    if (ImGui::DragFloat("Exposure", &exposure, 0.05f, 0.0f, 64.0f, "%.3f")) {
        m_editor_camera.setExposure(exposure);
    }

    ImGui::Separator();
    float moveSpeed = m_editor_camera.getMoveSpeed();
    if (ImGui::DragFloat("Move Speed", &moveSpeed, 0.05f, 0.0f, 256.0f, "%.3f")) {
        m_editor_camera.setMoveSpeed(moveSpeed);
        save();
    }

    float mouseSensitivity = m_editor_camera.getMouseSensitivity();
    if (ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity, 0.005f, 0.0f, 10.0f, "%.3f")) {
        m_editor_camera.setMouseSensitivity(mouseSensitivity);
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
            m_editor_camera.setMoveSpeed(editorCamera["MoveSpeed"].as<float>(m_editor_camera.getMoveSpeed()));
            m_editor_camera.setMouseSensitivity(
                editorCamera["MouseSensitivity"].as<float>(m_editor_camera.getMouseSensitivity()));
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
    root["EditorCamera"]["MoveSpeed"] = m_editor_camera.getMoveSpeed();
    root["EditorCamera"]["MouseSensitivity"] = m_editor_camera.getMouseSensitivity();

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
