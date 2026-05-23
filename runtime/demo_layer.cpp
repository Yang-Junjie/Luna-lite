#include "demo_layer.h"

#include "../LunaLite/asset/mesh_asset_loader.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene_renderer.h"

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

namespace lunalite::runtime {
DemoLayer::DemoLayer()
    : Layer("DemoLayer")
{}

void DemoLayer::onAttach()
{
    const auto cubeHandle = asset::MeshAssetLoader::loadObj("../../assets/stanford-bunny.obj");

    {
        auto entity = m_scene.createEntity();
        auto& transformComp = m_scene.addComponent<scene::TransformComponent>(entity);
        transformComp.scale = glm::vec3(8.0f, 8.0f, 8.0f);
        auto& meshComp = m_scene.addComponent<scene::MeshComponent>(entity);
        meshComp.mesh = cubeHandle;
    }

    {
        auto entity = m_scene.createEntity();
        auto& light = m_scene.addComponent<scene::DirectionalLightComponent>(entity);
        light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    }
}

void DemoLayer::onRender()
{
    core::Application::get().getSceneRenderer().render(m_scene);
}
} // namespace lunalite::runtime
