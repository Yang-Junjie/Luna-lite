#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/core/log.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/scene/scene_renderer.h"
#include "../LunaLite/scene/scene_serializer.h"
#include "demo_layer.h"

#include <filesystem>
#include <imgui.h>
#include <optional>
#include <system_error>
#include <utility>

namespace lunalite::core {
class Application;
}

namespace lunalite::runtime {
namespace {
std::optional<std::filesystem::path> findProjectFile(const std::filesystem::path& directory)
{
    std::error_code error;
    if (!std::filesystem::exists(directory, error) || !std::filesystem::is_directory(directory, error)) {
        LUNA_CORE_ERROR("Runtime project search directory does not exist: '{}'", directory.string());
        return std::nullopt;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            LUNA_CORE_ERROR("Failed to scan runtime project directory '{}': {}", directory.string(), error.message());
            return std::nullopt;
        }

        if (entry.is_regular_file(error) && entry.path().extension() == ".lunaproj") {
            return entry.path();
        }
    }

    if (error) {
        LUNA_CORE_ERROR("Failed to scan runtime project directory '{}': {}", directory.string(), error.message());
    }

    return std::nullopt;
}
} // namespace

DemoLayer::DemoLayer(std::filesystem::path project_search_path)
    : Layer("DemoLayer"),
      m_project_search_path(std::move(project_search_path))
{}

void DemoLayer::onAttach()
{
    const auto project_file = findProjectFile(m_project_search_path);
    if (!project_file) {
        LUNA_CORE_ERROR("Runtime failed to find a .lunaproj file next to '{}'", m_project_search_path.string());
        return;
    }

    auto& projectManager = project::ProjectManager::instance();
    if (!projectManager.loadProject(*project_file)) {
        LUNA_CORE_ERROR("Runtime failed to load project '{}'", project_file->string());
        return;
    }

    asset::AssetManager::get().loadProjectAssets();

    const auto& project_info = projectManager.getProjectInfo();
    const auto project_root_path = projectManager.getProjectRootPath();
    if (!project_info || !project_root_path) {
        LUNA_CORE_ERROR("Runtime failed to load scene: project info is missing");
        return;
    }

    if (project_info->start_scene.empty()) {
        LUNA_CORE_ERROR("Runtime failed to load scene: project '{}' has no StartScene", project_info->name);
        return;
    }

    const auto scene_path = *project_root_path / project_info->start_scene;
    if (!scene::SceneSerializer::deserialize(m_scene, scene_path)) {
        LUNA_CORE_ERROR("Runtime failed to load start scene '{}'", scene_path.string());
        return;
    }

    LUNA_CORE_INFO("Runtime loaded project '{}' and scene '{}'", project_info->name, scene_path.string());
    core::Application::get().switchRenderer(renderer::interface::RendererKind::Default);
}

void DemoLayer::onUpdate(core::Timestep dt)
{
    (void) dt;
}

void DemoLayer::onRender()
{
    core::Application::get().getSceneRenderer().onRenderRuntime(m_scene);
}

void DemoLayer::onImGuiRender()
{
    ImGui::Begin("Demo Layer");
    ImGui::Text("Hello World");
    ImGui::End();
}

} // namespace lunalite::runtime
