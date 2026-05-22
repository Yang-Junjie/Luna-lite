#include "../asset/asset_database.h"
#include "../asset/mesh_asset_loader.h"
#include "../renderer/default_renderer/renderer.h"
#include "../rhi/backend_factory.h"
#include "../scene/components.h"
#include "../scene/scene.h"
#include "../scene/scene_renderer.h"
#include "window.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main()
{
    auto instance = lunalite::rhi::BackendFactory::createInstance(lunalite::rhi::BackendType::OpenGL);

    lunalite::core::WindowCreateInfo window_info;
    window_info.width = 1'280;
    window_info.height = 720;
    window_info.title = "LunaLite";
    window_info.requirements = instance->getWindowRequirements();

    lunalite::core::Window window(window_info);

    if (!instance->init(window.getRHIWindowHandle())) {
        return -1;
    }

    lunalite::asset::AssetDatabase assets;
    const auto cubeHandle = lunalite::asset::MeshAssetLoader::loadObj("../../../assets/cube.obj", assets);
    lunalite::scene::Scene scene;

    lunalite::renderer::Renderer renderer(*instance);
    lunalite::scene::SceneRenderer scene_renderer(renderer, assets);

    {
        auto entity = scene.createEntity();
        scene.addComponent<lunalite::scene::TransformComponent>(entity);
        auto& meshComp = scene.addComponent<lunalite::scene::MeshComponent>(entity);
        meshComp.mesh = cubeHandle;
    }

    {
        auto entity = scene.createEntity();
        auto& light = scene.addComponent<lunalite::scene::DirectionalLightComponent>(entity);
        light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    }

    while (!window.shouldClose()) {
        window.onUpdate();
        scene_renderer.render(scene);
    }

    instance->shutdown();
    return 0;
}
