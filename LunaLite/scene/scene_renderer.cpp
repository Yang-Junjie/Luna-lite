#include "../asset/asset_database.h"
#include "../core/log.h"
#include "../renderer/interface/mesh.h"
#include "../renderer/interface/renderer.h"
#include "components.h"
#include "scene_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

namespace lunalite::scene {

SceneRenderer::SceneRenderer(renderer::interface::Renderer& renderer)
    : m_renderer(&renderer)
{}

void SceneRenderer::setRenderer(renderer::interface::Renderer& renderer)
{
    m_renderer = &renderer;
    LUNA_CORE_DEBUG("Scene renderer target changed");
}

void SceneRenderer::setViewportSize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    m_viewport_width = width;
    m_viewport_height = height;
}

void SceneRenderer::beginFrame()
{
    LUNA_ASSERT(m_renderer, "SceneRenderer has no renderer.");
    m_renderer->beginFrame();
}

void SceneRenderer::endFrame()
{
    LUNA_ASSERT(m_renderer, "SceneRenderer has no renderer.");
    m_renderer->endFrame();
}

void SceneRenderer::onRenderRuntime(const Scene& scene)
{
    LUNA_ASSERT(m_renderer, "SceneRenderer has no renderer.");

    const TransformComponent* cameraTransform = nullptr;
    const CameraComponent* cameraComponent = nullptr;
    const auto cameraView = scene.getRegistry().view<const TransformComponent, const CameraComponent>();
    for (const auto entity : cameraView) {
        const auto& candidate = cameraView.get<const CameraComponent>(entity);
        if (!candidate.primary && cameraComponent != nullptr) {
            continue;
        }

        cameraTransform = &cameraView.get<const TransformComponent>(entity);
        cameraComponent = &candidate;
        if (candidate.primary) {
            break;
        }
    }

    if (cameraTransform == nullptr || cameraComponent == nullptr) {
        return;
    }

    const float aspectRatio = static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height);
    const auto view = glm::inverse(cameraTransform->getTransform());
    const auto projection = cameraComponent->camera.getProjection(aspectRatio);
    renderScene(scene, view, projection, cameraTransform->translation);
}

void SceneRenderer::onRenderEditor(const Scene& scene, const renderer::interface::Camera& camera)
{
    LUNA_ASSERT(m_renderer, "SceneRenderer has no renderer.");

    const float aspectRatio = static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height);
    renderScene(scene, camera.getView(), camera.getProjection(aspectRatio), camera.getPosition());
}

void SceneRenderer::renderScene(const Scene& scene,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                const glm::vec3& cameraPosition)
{
    renderer::interface::SceneLighting lighting{};
    {
        const auto lightView = scene.getRegistry().view<const DirectionalLightComponent>();
        if (!lightView.empty()) {
            const auto& light = lightView.get<const DirectionalLightComponent>(*lightView.begin());
            lighting.directional_light_count = 1;
            lighting.directional_light.direction = light.direction;
            lighting.directional_light.ambient = light.ambient;
            lighting.directional_light.diffuse = light.diffuse;
            lighting.directional_light.specular = light.specular;
        }
    }

    m_renderer->setSceneLighting(lighting);
    m_renderer->setViewProjection(view, projection, cameraPosition);

    const auto meshView = scene.getRegistry().view<const TransformComponent, const MeshComponent>();
    for (const auto entity : meshView) {
        const auto& transform = meshView.get<const TransformComponent>(entity);
        const auto& meshComponent = meshView.get<const MeshComponent>(entity);
        if (!meshComponent.mesh.isValid()) {
            continue;
        }

        const auto* mesh = asset::AssetDatabase::get().get<renderer::interface::Mesh>(meshComponent.mesh);
        if (mesh == nullptr) {
            LUNA_CORE_ERROR("Entity {} has a null mesh asset", static_cast<uint32_t>(entity));
            continue;
        }

        m_renderer->renderMesh(*mesh, transform.getTransform());
    }
}
} // namespace lunalite::scene
