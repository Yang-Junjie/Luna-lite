#include "../LunaLite/asset/asset_database.h"
#include "../LunaLite/core/application.h"
#include "../LunaLite/core/log.h"
#include "../LunaLite/renderer/interface/mesh.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene_renderer.h"
#include "test_app_layer.h"

#include <cstdint>

#include <memory>
#include <vector>

namespace lunalite::test_app {
namespace {
renderer::interface::Vertex makeVertex(const glm::vec3& position, const glm::vec3& color)
{
    renderer::interface::Vertex vertex;
    vertex.position = position;
    vertex.normal = {0.0f, 0.0f, 1.0f};
    vertex.color = color;
    return vertex;
}

asset::AssetHandle createTriangleMesh()
{
    auto mesh = std::make_shared<renderer::interface::Mesh>();
    mesh->setVertices(std::vector<renderer::interface::Vertex>{
        makeVertex({-0.75f, -0.5f, 0.0f}, {1.0f, 0.2f, 0.2f}),
        makeVertex({0.75f, -0.5f, 0.0f}, {0.2f, 1.0f, 0.2f}),
        makeVertex({0.0f, 0.75f, 0.0f}, {0.2f, 0.35f, 1.0f}),
    });
    mesh->setIndices(std::vector<uint32_t>{0, 1, 2});

    const auto handle = asset::AssetDatabase::get().add(mesh);
    LUNA_ASSERT(handle.isValid(), "Failed to register test triangle mesh.");
    return handle;
}
} // namespace

TestAppLayer::TestAppLayer()
    : Layer("TestAppLayer")
{}

void TestAppLayer::onAttach()
{
    const auto triangleMesh = createTriangleMesh();

    auto triangle = m_scene.createEntity();
    m_scene.getComponent<scene::TagComponent>(triangle).tag = "Triangle";
    m_scene.addComponent<scene::MeshComponent>(triangle).mesh = triangleMesh;

    auto camera = m_scene.createEntity();
    m_scene.getComponent<scene::TagComponent>(camera).tag = "Main Camera";
    m_scene.getComponent<scene::TransformComponent>(camera).translation = {0.0f, 0.0f, 3.0f};
    auto& cameraComponent = m_scene.addComponent<scene::CameraComponent>(camera);
    cameraComponent.primary = true;
    cameraComponent.camera.setPerspective(glm::radians(45.0f), 0.1f, 100.0f);

    auto light = m_scene.createEntity();
    m_scene.getComponent<scene::TagComponent>(light).tag = "Directional Light";
    auto& lightComponent = m_scene.addComponent<scene::DirectionalLightComponent>(light);
    lightComponent.direction = {0.0f, 0.0f, -1.0f};
    lightComponent.ambient = {0.08f, 0.08f, 0.08f};
    lightComponent.diffuse = {0.9f, 0.9f, 0.9f};
}

void TestAppLayer::onRender()
{
    core::Application::get().getSceneRenderer().onRenderRuntime(m_scene);
}
} // namespace lunalite::test_app
