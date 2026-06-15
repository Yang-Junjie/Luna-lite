#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLiteTooling/commands/command_registry.h"
#include "../LunaLiteTooling/commands/project_commands.h"
#include "../LunaLiteTooling/context/tool_context.h"

#include <filesystem>
#include <iostream>

int main()
{
    using namespace lunalite;

    const auto projectRoot = std::filesystem::current_path() / "build" / "project_commands_test_project";
    std::error_code error;
    std::filesystem::remove_all(projectRoot, error);

    tooling::ToolContext context;
    tooling::CommandArgs createProjectArgs;
    createProjectArgs.set("project_root", projectRoot);
    createProjectArgs.set("name", std::string{"ProjectCommandsTestProject"});
    createProjectArgs.set("assets_path", std::filesystem::path{"Assets"});
    createProjectArgs.set("start_scene", std::filesystem::path{"Assets/RuntimeStart.lunascene"});
    const auto createProjectResult =
        tooling::CommandRegistry::get().execute(tooling::CreateProjectCommandId, context, createProjectArgs);
    if (!createProjectResult.success) {
        std::cerr << "project.create failed: " << createProjectResult.message << "\n";
        return 1;
    }
    if (!std::filesystem::exists(projectRoot / "ProjectCommandsTestProject.lunaproj") ||
        !std::filesystem::is_directory(projectRoot / "Assets")) {
        std::cerr << "project.create did not create expected project files.\n";
        return 1;
    }

    scene::Scene scene;
    context.setScene(scene);
    auto entity = scene.createEntity();
    scene.getComponent<scene::TagComponent>(entity).tag = "Unsaved";

    const auto scenePath = projectRoot / "Assets" / "EditorScene";
    tooling::CommandArgs createSceneArgs;
    createSceneArgs.set("scene_path", scenePath);
    const auto createSceneResult =
        tooling::CommandRegistry::get().execute(tooling::CreateSceneFileCommandId, context, createSceneArgs);
    if (!createSceneResult.success) {
        std::cerr << "scene.create_file failed: " << createSceneResult.message << "\n";
        return 1;
    }

    const auto savedScenePath = projectRoot / "Assets" / "EditorScene.lunascene";
    if (!std::filesystem::exists(savedScenePath) || !scene.getEntities().empty()) {
        std::cerr << "scene.create_file did not clear and save the scene.\n";
        return 1;
    }

    const auto& projectInfo = project::ProjectManager::instance().getProjectInfo();
    if (!projectInfo || projectInfo->start_scene.generic_string() != "Assets/RuntimeStart.lunascene" ||
        projectInfo->last_open_scene.generic_string() != "Assets/EditorScene.lunascene") {
        std::cerr << "scene.create_file did not preserve start scene and update last open scene.\n";
        return 1;
    }

    entity = scene.createEntity();
    scene.getComponent<scene::TagComponent>(entity).tag = "Saved";
    tooling::CommandArgs saveSceneArgs;
    saveSceneArgs.set("scene_path", savedScenePath);
    const auto saveSceneResult =
        tooling::CommandRegistry::get().execute(tooling::SaveSceneFileCommandId, context, saveSceneArgs);
    if (!saveSceneResult.success) {
        std::cerr << "scene.save_file failed: " << saveSceneResult.message << "\n";
        return 1;
    }

    scene.clear();
    tooling::CommandArgs openSceneArgs;
    openSceneArgs.set("scene_path", savedScenePath);
    const auto openSceneResult =
        tooling::CommandRegistry::get().execute(tooling::OpenSceneFileCommandId, context, openSceneArgs);
    if (!openSceneResult.success) {
        std::cerr << "scene.open_file failed: " << openSceneResult.message << "\n";
        return 1;
    }
    bool foundSavedEntity = false;
    for (const auto loadedEntity : scene.getEntities()) {
        if (scene.hasComponent<scene::TagComponent>(loadedEntity) &&
            scene.getComponent<scene::TagComponent>(loadedEntity).tag == "Saved") {
            foundSavedEntity = true;
        }
    }
    if (!foundSavedEntity) {
        std::cerr << "scene.open_file did not load the saved scene content.\n";
        return 1;
    }
    const auto& openedProjectInfo = project::ProjectManager::instance().getProjectInfo();
    if (!openedProjectInfo || openedProjectInfo->last_open_scene.generic_string() != "Assets/EditorScene.lunascene") {
        std::cerr << "scene.open_file did not update last open scene.\n";
        return 1;
    }

    const auto saveProjectResult = tooling::CommandRegistry::get().execute(tooling::SaveProjectCommandId, context);
    if (!saveProjectResult.success) {
        std::cerr << "project.save failed: " << saveProjectResult.message << "\n";
        return 1;
    }

    tooling::CommandArgs openProjectArgs;
    openProjectArgs.set("project_path", projectRoot / "ProjectCommandsTestProject.lunaproj");
    const auto openProjectResult =
        tooling::CommandRegistry::get().execute(tooling::OpenProjectCommandId, context, openProjectArgs);
    if (!openProjectResult.success) {
        std::cerr << "project.open failed: " << openProjectResult.message << "\n";
        return 1;
    }

    const auto reopenedRoot = project::ProjectManager::instance().getProjectRootPath();
    const auto& reopenedInfo = project::ProjectManager::instance().getProjectInfo();
    if (!reopenedRoot || reopenedRoot->lexically_normal() != projectRoot.lexically_normal() || !reopenedInfo ||
        reopenedInfo->start_scene.generic_string() != "Assets/RuntimeStart.lunascene" ||
        reopenedInfo->last_open_scene.generic_string() != "Assets/EditorScene.lunascene") {
        std::cerr << "project.open did not restore project state.\n";
        return 1;
    }

    std::cout << "Project commands created, opened, and saved projects and scenes.\n";
    return 0;
}
