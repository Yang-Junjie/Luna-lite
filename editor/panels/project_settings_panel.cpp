#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/platform/common/file_dialogs.h"
#include "../../LunaLite/project/project_manager.h"
#include "project_settings_panel.h"

#include <cfloat>
#include <cstring>

#include <algorithm>
#include <array>
#include <imgui.h>
#include <string>

namespace lunalite::editor {
namespace {
template <size_t Size> void copyToBuffer(std::array<char, Size>& buffer, const std::string& value)
{
    buffer.fill('\0');
    const auto copySize = std::min(value.size(), buffer.size() - 1);
    std::memcpy(buffer.data(), value.data(), copySize);
}

template <size_t Size> void copyToBuffer(std::array<char, Size>& buffer, const std::filesystem::path& value)
{
    copyToBuffer(buffer, value.generic_string());
}

std::filesystem::path projectRelativePath(const std::filesystem::path& path)
{
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot || path.empty()) {
        return path;
    }

    const auto relativePath = path.lexically_relative(*projectRoot);
    return relativePath.empty() ? path : relativePath;
}

bool drawTextField(const char* label, std::string& value)
{
    std::array<char, 256> buffer{};
    copyToBuffer(buffer, value);
    if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
        return false;
    }

    value = buffer.data();
    return true;
}

bool drawMultilineField(const char* label, std::string& value)
{
    std::array<char, 1'024> buffer{};
    copyToBuffer(buffer, value);
    if (!ImGui::InputTextMultiline(
            label, buffer.data(), buffer.size(), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4))) {
        return false;
    }

    value = buffer.data();
    return true;
}

bool drawPathField(const char* label, std::filesystem::path& value)
{
    std::array<char, 512> buffer{};
    copyToBuffer(buffer, value);
    if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
        return false;
    }

    value = std::filesystem::path{buffer.data()};
    return true;
}

void drawReadOnlyPathField(const char* label, const std::filesystem::path& value)
{
    std::array<char, 512> buffer{};
    copyToBuffer(buffer, value);
    ImGui::InputText(label, buffer.data(), buffer.size(), ImGuiInputTextFlags_ReadOnly);
}

bool sameProjectInfo(const project::ProjectInfo& lhs, const project::ProjectInfo& rhs)
{
    return lhs.name == rhs.name && lhs.version == rhs.version && lhs.author == rhs.author &&
           lhs.description == rhs.description && lhs.start_scene == rhs.start_scene &&
           lhs.last_open_scene == rhs.last_open_scene && lhs.assets_path == rhs.assets_path;
}

void drawPathStatus(const std::filesystem::path& path)
{
    if (path.empty()) {
        ImGui::TextDisabled("Not set");
        return;
    }

    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    const auto absolutePath = path.is_absolute() || !projectRoot ? path : (*projectRoot / path).lexically_normal();
    if (std::filesystem::exists(absolutePath)) {
        ImGui::TextDisabled("%s", absolutePath.string().c_str());
    } else {
        ImGui::TextColored(ImVec4{1.0f, 0.45f, 0.25f, 1.0f}, "Missing: %s", absolutePath.string().c_str());
    }
}

} // namespace

ProjectSettingsPanel::ProjectSettingsPanel(const std::filesystem::path& currentScenePath)
    : m_current_scene_path(currentScenePath)
{}

void ProjectSettingsPanel::onImGuiRender()
{
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    const auto& projectInfo = project::ProjectManager::instance().getProjectInfo();
    if (projectRoot != m_loaded_project_root ||
        (!m_dirty && projectInfo && !sameProjectInfo(m_edit_info, *projectInfo))) {
        syncFromProject();
    }

    ImGui::Begin("Project Settings");

    if (!m_loaded || !projectRoot) {
        ImGui::TextUnformatted("No project loaded");
        ImGui::End();
        return;
    }

    const auto projectRootText = projectRoot->string();
    ImGui::TextDisabled("%s", projectRootText.c_str());
    ImGui::Separator();

    if (drawTextField("Name", m_edit_info.name)) {
        markDirty();
    }
    if (drawTextField("Version", m_edit_info.version)) {
        markDirty();
    }
    if (drawTextField("Author", m_edit_info.author)) {
        markDirty();
    }
    if (drawMultilineField("Description", m_edit_info.description)) {
        markDirty();
    }

    ImGui::Separator();
    if (drawPathField("Assets Path", m_edit_info.assets_path)) {
        markDirty();
    }
    drawPathStatus(m_edit_info.assets_path);

    ImGui::Separator();
    if (drawPathField("Start Scene", m_edit_info.start_scene)) {
        markDirty();
    }
    drawPathStatus(m_edit_info.start_scene);
    if (ImGui::Button("Choose")) {
        chooseStartScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Use Current")) {
        setStartSceneToCurrentScene();
    }

    ImGui::Separator();
    drawReadOnlyPathField("Last Open Scene", m_edit_info.last_open_scene);
    drawPathStatus(m_edit_info.last_open_scene);

    ImGui::Separator();
    if (ImGui::Button("Save")) {
        save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        reset();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_dirty ? "Modified" : "Saved");

    ImGui::End();
}

void ProjectSettingsPanel::syncFromProject()
{
    m_loaded_project_root = project::ProjectManager::instance().getProjectRootPath();
    const auto& projectInfo = project::ProjectManager::instance().getProjectInfo();
    if (projectInfo) {
        m_edit_info = *projectInfo;
        m_loaded = true;
    } else {
        m_edit_info = {};
        m_loaded = false;
    }
    m_dirty = false;
}

void ProjectSettingsPanel::markDirty()
{
    m_dirty = true;
}

void ProjectSettingsPanel::save()
{
    if (m_edit_info.name.empty()) {
        LUNA_CORE_ERROR("Failed to save project settings: project name is empty");
        return;
    }
    if (m_edit_info.assets_path.empty() || m_edit_info.assets_path.is_absolute()) {
        LUNA_CORE_ERROR("Failed to save project settings: assets path must be a project-relative path");
        return;
    }

    auto& projectManager = project::ProjectManager::instance();
    const auto& previousInfo = projectManager.getProjectInfo();
    const bool assetsPathChanged = previousInfo && previousInfo->assets_path != m_edit_info.assets_path;

    projectManager.setProjectInfo(m_edit_info);
    if (!projectManager.saveProject()) {
        LUNA_CORE_ERROR("Failed to save project settings");
        return;
    }

    if (assetsPathChanged && !asset::AssetManager::get().loadProjectAssets()) {
        LUNA_CORE_ERROR("Project settings saved, but assets failed to reload");
    }

    m_dirty = false;
    syncFromProject();
}

void ProjectSettingsPanel::reset()
{
    syncFromProject();
}

void ProjectSettingsPanel::chooseStartScene()
{
    const auto scenePath = FileDialogs::openFile("Luna Scene\0*.lunascene\0", {});
    if (scenePath.empty()) {
        return;
    }
    m_edit_info.start_scene = projectRelativePath(scenePath);
    markDirty();
}

void ProjectSettingsPanel::setStartSceneToCurrentScene()
{
    if (m_current_scene_path.empty()) {
        return;
    }
    m_edit_info.start_scene = projectRelativePath(m_current_scene_path);
    markDirty();
}

} // namespace lunalite::editor
