#include "../LunaLite/asset/mesh_asset_loader.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/core/application_event.h"
#include "../LunaLite/core/input.h"
#include "../LunaLite/core/key_event.h"
#include "../LunaLite/core/log.h"
#include "../LunaLite/core/mouse_event.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene_renderer.h"
#include "demo_layer.h"

#include <glm/geometric.hpp>
#include <glm/glm.hpp>

namespace lunalite::core {
class Application;
}

namespace lunalite::runtime {
DemoLayer::DemoLayer()
    : Layer("DemoLayer")
{}

void DemoLayer::onAttach()
{
    const auto obj_handle = asset::MeshAssetLoader::loadObj("../../assets/stanford-bunny.obj");

    {
        auto entity = m_scene.createEntity();
        m_model_entity = entity;
        auto& transformComp = m_scene.addComponent<scene::TransformComponent>(entity);
        transformComp.scale = glm::vec3(8.0f, 8.0f, 8.0f);
        auto& meshComp = m_scene.addComponent<scene::MeshComponent>(entity);
        meshComp.mesh = obj_handle;
    }

    {
        auto entity = m_scene.createEntity();
        auto& light = m_scene.addComponent<scene::DirectionalLightComponent>(entity);
        light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    }

    {
        auto entity = m_scene.createEntity();
        auto& transform = m_scene.addComponent<scene::TransformComponent>(entity);
        transform.translation = glm::vec3(3.0f, 2.0f, 5.0f);
        transform.rotation = glm::vec3(glm::radians(-20.0f), glm::radians(30.0f), 0.0f);
        m_scene.addComponent<scene::CameraComponent>(entity);
    }

    core::Application::get().switchRenderer(renderer::interface::RendererKind::Default);
}

void DemoLayer::onUpdate(core::Timestep dt)
{
    auto& transform = m_scene.getComponent<scene::TransformComponent>(m_model_entity);
    transform.rotation.y += dt.getMilliseconds() * 0.01f;
}

void DemoLayer::onRender()
{
    core::Application::get().getSceneRenderer().onRenderRuntime(m_scene);
}

} // namespace lunalite::runtime
