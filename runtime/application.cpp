#include "../LunaLite/asset/mesh_asset_loader.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/core/layer.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/scene_renderer.h"

#include <glm/glm.hpp>
#include <memory>

namespace {
class DemoLayer final : public lunalite::core::Layer {
public:
    DemoLayer()
        : Layer("DemoLayer")
    {}

    void onAttach() override
    {
        const auto cubeHandle = lunalite::asset::MeshAssetLoader::loadObj("../../assets/stanford-bunny.obj");

        {
            auto entity = m_scene.createEntity();
            auto& transformComp = m_scene.addComponent<lunalite::scene::TransformComponent>(entity);
            transformComp.scale = glm::vec3(8.0f, 8.0f, 8.0f);
            auto& meshComp = m_scene.addComponent<lunalite::scene::MeshComponent>(entity);
            meshComp.mesh = cubeHandle;
        }

        {
            auto entity = m_scene.createEntity();
            auto& light = m_scene.addComponent<lunalite::scene::DirectionalLightComponent>(entity);
            light.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
        }
    }

    void onRender() override
    {
        lunalite::core::Application::get().getSceneRenderer().render(m_scene);
    }

private:
    lunalite::scene::Scene m_scene;
};
} // namespace

namespace lunalite::core {
Application* createApplication(int argc, char** argv)
{
    ApplicationCreateInfo info;
    info.name = "LunaLite";
    info.width = 1'280;
    info.height = 720;
    info.backend = rhi::BackendType::OpenGL;

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<DemoLayer>());

    return app;
}
} // namespace lunalite::core
