#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/project/project_manager.h"
#include "../../LunaLite/scene/scene.h"
#include "../../LunaLite/scene/scene_serializer.h"
#include "command_registry.h"
#include "project_commands.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace lunalite::tooling {
namespace {
std::filesystem::path ensureSceneExtension(std::filesystem::path path)
{
    if (path.extension().empty()) {
        path.replace_extension(scene::SceneSerializer::FileExtension);
    }
    return path;
}

std::filesystem::path projectRelativePath(const std::filesystem::path& path)
{
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot) {
        return path;
    }

    const auto relativePath = path.lexically_relative(*projectRoot);
    return relativePath.empty() ? path : relativePath;
}

CommandResult projectResult(std::string message)
{
    auto result = CommandResult::ok(std::move(message));
    if (const auto projectRoot = project::ProjectManager::instance().getProjectRootPath()) {
        result.set("project_root", *projectRoot);
    }
    if (const auto& info = project::ProjectManager::instance().getProjectInfo()) {
        result.set("project_name", info->name);
        result.set("assets_path", info->assets_path);
        result.set("start_scene", info->start_scene);
    }
    return result;
}

CommandResult createProject(const CommandArgs& args)
{
    const auto projectRoot = args.get<std::filesystem::path>("project_root");
    if (!projectRoot || projectRoot->empty()) {
        return CommandResult::fail("project.create requires project_root");
    }

    project::ProjectInfo info;
    if (const auto name = args.get<std::string>("name")) {
        info.name = *name;
    } else {
        info.name = projectRoot->filename().string();
    }
    if (const auto version = args.get<std::string>("version")) {
        info.version = *version;
    }
    if (const auto author = args.get<std::string>("author")) {
        info.author = *author;
    }
    if (const auto description = args.get<std::string>("description")) {
        info.description = *description;
    }
    if (const auto assetsPath = args.get<std::filesystem::path>("assets_path")) {
        info.assets_path = *assetsPath;
    } else {
        info.assets_path = "Assets";
    }
    if (const auto startScene = args.get<std::filesystem::path>("start_scene")) {
        info.start_scene = *startScene;
    }

    if (!project::ProjectManager::instance().createProject(*projectRoot, info)) {
        return CommandResult::fail("Failed to create project");
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        return CommandResult::fail("Project was created but assets failed to load");
    }

    return projectResult("Project created");
}

CommandResult openProject(const CommandArgs& args)
{
    const auto projectPath = args.get<std::filesystem::path>("project_path");
    if (!projectPath || projectPath->empty()) {
        return CommandResult::fail("project.open requires project_path");
    }

    if (!project::ProjectManager::instance().loadProject(*projectPath)) {
        return CommandResult::fail("Failed to open project");
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        return CommandResult::fail("Project was opened but assets failed to load");
    }

    return projectResult("Project opened");
}

CommandResult saveProject()
{
    if (!project::ProjectManager::instance().saveProject()) {
        return CommandResult::fail("Failed to save project");
    }

    return projectResult("Project saved");
}

CommandResult createSceneFile(ToolContext& context, const CommandArgs& args)
{
    auto* scene = context.scene();
    if (scene == nullptr) {
        return CommandResult::fail("scene.create_file requires an active scene");
    }

    const auto scenePathArg = args.get<std::filesystem::path>("scene_path");
    if (!scenePathArg || scenePathArg->empty()) {
        return CommandResult::fail("scene.create_file requires scene_path");
    }

    const auto scenePath = ensureSceneExtension(*scenePathArg);
    scene->clear();
    if (!scene::SceneSerializer::serialize(*scene, scenePath)) {
        return CommandResult::fail("Failed to create scene file");
    }

    auto& projectManager = project::ProjectManager::instance();
    if (const auto& projectInfo = projectManager.getProjectInfo()) {
        auto info = *projectInfo;
        info.start_scene = projectRelativePath(scenePath);
        projectManager.setProjectInfo(info);
        projectManager.saveProject();
    }

    auto result = CommandResult::ok("Scene file created");
    result.set("scene_path", scenePath);
    return result;
}

CommandResult openSceneFile(ToolContext& context, const CommandArgs& args)
{
    auto* scene = context.scene();
    if (scene == nullptr) {
        return CommandResult::fail("scene.open_file requires an active scene");
    }

    const auto scenePath = args.get<std::filesystem::path>("scene_path");
    if (!scenePath || scenePath->empty()) {
        return CommandResult::fail("scene.open_file requires scene_path");
    }

    if (!scene::SceneSerializer::deserialize(*scene, *scenePath)) {
        return CommandResult::fail("Failed to open scene file");
    }

    auto result = CommandResult::ok("Scene file opened");
    result.set("scene_path", *scenePath);
    return result;
}

CommandResult saveSceneFile(ToolContext& context, const CommandArgs& args)
{
    auto* scene = context.scene();
    if (scene == nullptr) {
        return CommandResult::fail("scene.save_file requires an active scene");
    }

    const auto scenePathArg = args.get<std::filesystem::path>("scene_path");
    if (!scenePathArg || scenePathArg->empty()) {
        return CommandResult::fail("scene.save_file requires scene_path");
    }

    const auto scenePath = ensureSceneExtension(*scenePathArg);
    if (!scene::SceneSerializer::serialize(*scene, scenePath)) {
        return CommandResult::fail("Failed to save scene file");
    }

    auto result = CommandResult::ok("Scene file saved");
    result.set("scene_path", scenePath);
    return result;
}
} // namespace

std::string_view CreateProjectCommand::id() const
{
    return CreateProjectCommandId;
}

std::string_view CreateProjectCommand::label() const
{
    return "Create Project";
}

std::string_view CreateProjectCommand::category() const
{
    return "Project";
}

CommandResult CreateProjectCommand::execute(ToolContext&, const CommandArgs& args)
{
    return createProject(args);
}

std::string_view OpenProjectCommand::id() const
{
    return OpenProjectCommandId;
}

std::string_view OpenProjectCommand::label() const
{
    return "Open Project";
}

std::string_view OpenProjectCommand::category() const
{
    return "Project";
}

CommandResult OpenProjectCommand::execute(ToolContext&, const CommandArgs& args)
{
    return openProject(args);
}

std::string_view SaveProjectCommand::id() const
{
    return SaveProjectCommandId;
}

std::string_view SaveProjectCommand::label() const
{
    return "Save Project";
}

std::string_view SaveProjectCommand::category() const
{
    return "Project";
}

CommandResult SaveProjectCommand::execute(ToolContext&, const CommandArgs&)
{
    return saveProject();
}

std::string_view CreateSceneFileCommand::id() const
{
    return CreateSceneFileCommandId;
}

std::string_view CreateSceneFileCommand::label() const
{
    return "Create Scene File";
}

std::string_view CreateSceneFileCommand::category() const
{
    return "Scene";
}

CommandResult CreateSceneFileCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return createSceneFile(context, args);
}

std::string_view OpenSceneFileCommand::id() const
{
    return OpenSceneFileCommandId;
}

std::string_view OpenSceneFileCommand::label() const
{
    return "Open Scene File";
}

std::string_view OpenSceneFileCommand::category() const
{
    return "Scene";
}

CommandResult OpenSceneFileCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return openSceneFile(context, args);
}

std::string_view SaveSceneFileCommand::id() const
{
    return SaveSceneFileCommandId;
}

std::string_view SaveSceneFileCommand::label() const
{
    return "Save Scene File";
}

std::string_view SaveSceneFileCommand::category() const
{
    return "Scene";
}

CommandResult SaveSceneFileCommand::execute(ToolContext& context, const CommandArgs& args)
{
    return saveSceneFile(context, args);
}

void registerProjectCommands(CommandRegistry& registry)
{
    registry.registerCommand(std::make_unique<CreateProjectCommand>());
    registry.registerCommand(std::make_unique<OpenProjectCommand>());
    registry.registerCommand(std::make_unique<SaveProjectCommand>());
    registry.registerCommand(std::make_unique<CreateSceneFileCommand>());
    registry.registerCommand(std::make_unique<OpenSceneFileCommand>());
    registry.registerCommand(std::make_unique<SaveSceneFileCommand>());
}

} // namespace lunalite::tooling
