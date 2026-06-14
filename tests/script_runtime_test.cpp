#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/renderer/components/renderer_components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/scene_components.h"
#include "../LunaLite/script/script_components.h"

#include <cmath>

#include <filesystem>
#include <fstream>
#include <glm/gtc/quaternion.hpp>
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

    project::ProjectInfo projectInfo;
    projectInfo.name = "script_runtime_test_project";
    projectInfo.assets_path = "Assets";

    if (!project::ProjectManager::instance().createProject(projectRoot, projectInfo)) {
        std::cerr << "Failed to create script runtime test project.\n";
        return 1;
    }

    std::filesystem::create_directories(projectRoot / projectInfo.assets_path / "Scripts");

    {
        std::ofstream script(projectRoot / projectInfo.assets_path / "Scripts" / "rotate.lua");
        script << R"(
return {
    on_create = function(entity)
        Log.info("script runtime test")

        local child = Scene.create_entity("child")
        local found_child = Scene.find_entity_by_tag("child")
        if found_child:is_valid() then
            found_child:destroy()
        else
            entity:set_tag("failed to find child")
            return
        end

        if Input.is_key_pressed("W") then
            entity:set_tag("unexpected input")
            return
        end

        entity:add_camera()
        entity:set_mesh_renderer("12712907297239299619")
        entity:set_tag("started")
    end,

    on_update = function(entity, dt)
        local x, y, z = entity:get_translation()
        entity:set_translation(x + Time.delta_time, y + Time.elapsed_time, z)

        local rx, ry, rz = entity:get_rotation()
        entity:set_rotation(rx, ry + dt, rz)

        if entity:has_camera() then
            entity:remove_camera()
        end

        if entity:has_mesh_renderer() then
            entity:remove_mesh_renderer()
        end
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

    const auto scriptHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/Scripts/rotate.lua");
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
    if (!scene.hasComponent<scene::CameraComponent>(entity) ||
        !scene.hasComponent<scene::MeshRendererComponent>(entity)) {
        std::cerr << "Lua component API did not add components.\n";
        return 1;
    }
    if (scene.getComponent<scene::MeshRendererComponent>(entity).mesh !=
        asset::AssetHandle{12'712'907'297'239'299'619ull}) {
        std::cerr << "Lua component API did not preserve string mesh handle.\n";
        return 1;
    }
    if (scene.getEntities().size() != 1) {
        std::cerr << "Lua scene API did not destroy created child entity.\n";
        return 1;
    }

    scene.onUpdateRuntime(core::Timestep{0.5f});
    const auto& transform = scene.getComponent<scene::TransformComponent>(entity);
    const auto rotation = glm::eulerAngles(transform.rotation);
    if (!near(transform.translation.x, 0.5f) || !near(transform.translation.y, 0.5f) || !near(rotation.y, 0.5f)) {
        std::cerr << "Lua on_update did not update transform.\n";
        return 1;
    }
    if (scene.hasComponent<scene::CameraComponent>(entity) ||
        scene.hasComponent<scene::MeshRendererComponent>(entity)) {
        std::cerr << "Lua component API did not remove components.\n";
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
