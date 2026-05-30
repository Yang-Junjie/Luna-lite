#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"

#include <cmath>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
bool near(float a, float b)
{
    return std::abs(a - b) < 0.0001f;
}
} // namespace

int main()
{
    using namespace lunalite;

    const auto projectRoot = std::filesystem::current_path() / "build" / "script_runtime_test_project";
    std::filesystem::create_directories(projectRoot / "Scripts");

    project::ProjectInfo projectInfo;
    projectInfo.name = "script_runtime_test_project";
    projectInfo.assets_path = "Assets";

    if (!project::ProjectManager::instance().createProject(projectRoot, projectInfo)) {
        std::cerr << "Failed to create script runtime test project.\n";
        return 1;
    }

    {
        std::ofstream script(projectRoot / "Scripts" / "rotate.lua");
        script << R"(
return {
    on_create = function(entity)
        entity:set_tag("started")
    end,

    on_update = function(entity, dt)
        local x, y, z = entity:get_translation()
        entity:set_translation(x + dt, y + dt * 2.0, z)

        local rx, ry, rz = entity:get_rotation()
        entity:set_rotation(rx, ry + dt, rz)
    end,

    on_destroy = function(entity)
        entity:set_tag("stopped")
    end
}
)";
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        std::cerr << "Failed to load script runtime test assets.\n";
        return 1;
    }

    const auto scriptHandle = asset::AssetManager::get().getHandleByRelativePath("Scripts/rotate.lua");
    if (!scriptHandle.isValid()) {
        std::cerr << "Failed to find script asset handle.\n";
        return 1;
    }

    scene::Scene scene;
    const auto entity = scene.createEntity();
    auto& script = scene.addComponent<scene::ScriptComponent>(entity);
    script.scripts.push_back({scriptHandle, true});

    scene.onRuntimeStart();
    if (scene.getComponent<scene::TagComponent>(entity).tag != "started") {
        std::cerr << "Lua on_create did not update tag.\n";
        return 1;
    }

    scene.onUpdateRuntime(core::Timestep{0.5f});
    const auto& transform = scene.getComponent<scene::TransformComponent>(entity);
    if (!near(transform.translation.x, 0.5f) || !near(transform.translation.y, 1.0f) ||
        !near(transform.rotation.y, 0.5f)) {
        std::cerr << "Lua on_update did not update transform.\n";
        return 1;
    }

    scene.onRuntimeStop();
    if (scene.getComponent<scene::TagComponent>(entity).tag != "stopped") {
        std::cerr << "Lua on_destroy did not update tag.\n";
        return 1;
    }

    std::cout << "Script runtime executed Lua lifecycle functions.\n";
    return 0;
}
