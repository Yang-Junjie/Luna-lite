#include "../asset/asset_database.h"
#include "../core/log.h"
#include "../renderer/interface/model.h"
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
    renderScene(scene, view, projection, cameraTransform->translation, cameraComponent->camera.getExposure());
}

void SceneRenderer::onRenderEditor(const Scene& scene, const renderer::interface::Camera& camera)
{
    LUNA_ASSERT(m_renderer, "SceneRenderer has no renderer.");

    const float aspectRatio = static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height);
    renderScene(scene, camera.getView(), camera.getProjection(aspectRatio), camera.getPosition(), camera.getExposure());
}

void SceneRenderer::renderScene(const Scene& scene,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                const glm::vec3& cameraPosition,
                                float exposure)
{
    renderer::interface::RenderLighting lighting{};
    lighting.environment_map = scene.getSettings().environment_map;
    lighting.environment_intensity = scene.getSettings().environment_intensity;
    {
        const auto lightView = scene.getRegistry().view<const DirectionalLightComponent>();
        if (!lightView.empty()) {
            const auto& light = lightView.get<const DirectionalLightComponent>(*lightView.begin());
            lighting.directional_light_count = 1;
            lighting.directional_light.direction = light.direction;
            lighting.directional_light.radiance = light.color * light.intensity;
        }
    }

    m_renderer->setLighting(lighting);
    m_renderer->setViewProjection(view, projection, cameraPosition, exposure);

    const auto modelView = scene.getRegistry().view<const TransformComponent, const ModelComponent>();
    for (const auto entity : modelView) {
        const auto& transform = modelView.get<const TransformComponent>(entity);
        const auto& modelComponent = modelView.get<const ModelComponent>(entity);
        if (!modelComponent.model.isValid()) {
            continue;
        }

        const auto* model = asset::AssetDatabase::get().get<renderer::interface::Model>(modelComponent.model);
        if (model == nullptr) {
            LUNA_CORE_ERROR("Entity {} has a null model asset", static_cast<uint32_t>(entity));
            continue;
        }

        m_renderer->renderModel(*model, transform.getTransform());
    }
}
} // namespace lunalite::scene
