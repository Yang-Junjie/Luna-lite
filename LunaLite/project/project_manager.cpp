#include "../core/log.h"
#include "project_manager.h"

#include <fstream>
#include <system_error>
#include <yaml-cpp/yaml.h>

namespace lunalite::project {

bool ProjectManager::loadProject(const std::filesystem::path& project_file_path)
{
    m_project_file_path = std::filesystem::absolute(project_file_path);
    m_project_root_path = m_project_file_path->parent_path();
    LUNA_CORE_INFO("Loading project from '{}'", m_project_file_path->string());
    return deserializeProject();
}

std::optional<std::filesystem::path> ProjectManager::getProjectRootPath() const
{
    return m_project_root_path;
}

const std::optional<ProjectInfo>& ProjectManager::getProjectInfo() const
{
    return m_project_info;
}

void ProjectManager::setProjectInfo(const ProjectInfo& info)
{
    m_project_info = info;
}

bool ProjectManager::saveProject()
{
    return serializeProject();
}

bool ProjectManager::createProject(const std::filesystem::path& project_root_path, const ProjectInfo& info)
{
    if (info.name.empty()) {
        LUNA_CORE_ERROR("Failed to create project: project name is empty");
        return false;
    }

    m_project_root_path = std::filesystem::absolute(project_root_path);
    m_project_file_path = *m_project_root_path / (info.name + ".lunaproj");
    m_project_info = info;
    LUNA_CORE_INFO("Creating project '{}' at '{}'", info.name, m_project_root_path->string());

    std::error_code error;
    std::filesystem::create_directories(*m_project_root_path, error);
    if (error) {
        LUNA_CORE_ERROR("Failed to create project directory '{}': {}", m_project_root_path->string(), error.message());
        return false;
    }

    if (!info.assets_path.empty()) {
        std::filesystem::create_directories(*m_project_root_path / info.assets_path, error);
        if (error) {
            LUNA_CORE_ERROR("Failed to create assets directory '{}': {}",
                            (*m_project_root_path / info.assets_path).string(),
                            error.message());
            return false;
        }
    }

    return serializeProject();
}

bool ProjectManager::serializeProject()
{
    if (!m_project_info || !m_project_file_path) {
        LUNA_CORE_ERROR("Failed to serialize project: no project is loaded");
        return false;
    }

    YAML::Node root;
    YAML::Node project;
    project["Name"] = m_project_info->name;
    project["Version"] = m_project_info->version;
    project["Author"] = m_project_info->author;
    project["Description"] = m_project_info->description;
    project["StartScene"] = m_project_info->start_scene.generic_string();
    project["LastOpenScene"] = m_project_info->last_open_scene.generic_string();
    project["AssetsPath"] = m_project_info->assets_path.generic_string();
    root["Project"] = project;

    std::ofstream out(*m_project_file_path);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open project file for writing: '{}'", m_project_file_path->string());
        return false;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write project file: '{}'", m_project_file_path->string());
        return false;
    }

    LUNA_CORE_INFO("Saved project '{}' to '{}'", m_project_info->name, m_project_file_path->string());
    return true;
}

bool ProjectManager::deserializeProject()
{
    if (!m_project_file_path) {
        LUNA_CORE_ERROR("Failed to deserialize project: project file path is empty");
        return false;
    }

    try {
        const YAML::Node root = YAML::LoadFile(m_project_file_path->string());
        const YAML::Node project = root["Project"];
        if (!project || !project["Name"]) {
            LUNA_CORE_ERROR("Failed to deserialize project '{}': missing Project/Name", m_project_file_path->string());
            return false;
        }

        ProjectInfo info;
        info.name = project["Name"].as<std::string>();
        if (project["Version"]) {
            info.version = project["Version"].as<std::string>();
        }
        if (project["Author"]) {
            info.author = project["Author"].as<std::string>();
        }
        if (project["Description"]) {
            info.description = project["Description"].as<std::string>();
        }
        if (project["StartScene"]) {
            info.start_scene = project["StartScene"].as<std::string>();
        }
        if (project["LastOpenScene"]) {
            info.last_open_scene = project["LastOpenScene"].as<std::string>();
        }
        if (project["AssetsPath"]) {
            info.assets_path = project["AssetsPath"].as<std::string>();
        }

        m_project_info = info;
        LUNA_CORE_INFO("Loaded project '{}' from '{}' (assets: '{}', start scene: '{}', last open scene: '{}')",
                       m_project_info->name,
                       m_project_file_path->string(),
                       m_project_info->assets_path.string(),
                       m_project_info->start_scene.string(),
                       m_project_info->last_open_scene.string());
        return true;
    } catch (const YAML::Exception& error) {
        LUNA_CORE_ERROR("Failed to deserialize project '{}': {}", m_project_file_path->string(), error.what());
        return false;
    } catch (const std::filesystem::filesystem_error& error) {
        LUNA_CORE_ERROR("Failed to deserialize project '{}': {}", m_project_file_path->string(), error.what());
        return false;
    }
}

} // namespace lunalite::project
