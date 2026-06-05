#include "../asset/asset_database.h"
#include "../core/log.h"
#include "../renderer/interface/model.h"
#include "../renderer/interface/renderer.h"
#include "components.h"
#include "scene_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lunalite::scene {
namespace {
glm::vec3 directionalLightDirection(const glm::quat& rotation)
{
    const auto direction = rotation * glm::vec3{0.0f, -1.0f, 0.0f};
    const auto length = glm::length(direction);
    if (length <= 0.0001f) {
        return glm::vec3{0.0f, -1.0f, 0.0f};
    }

    return direction / length;
}

bool decomposePose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation)
{
    translation = glm::vec3{matrix[3]};

    glm::vec3 basis0{matrix[0]};
    glm::vec3 basis1{matrix[1]};
    glm::vec3 basis2{matrix[2]};

    const auto scaleX = glm::length(basis0);
    const auto scaleY = glm::length(basis1);
    const auto scaleZ = glm::length(basis2);
    constexpr float minScale = 0.000001f;
    if (scaleX <= minScale || scaleY <= minScale || scaleZ <= minScale) {
        return false;
    }

    basis0 /= scaleX;
    basis1 /= scaleY;
    basis2 /= scaleZ;

    if (glm::determinant(glm::mat3{basis0, basis1, basis2}) < 0.0f) {
        if (scaleX >= scaleY && scaleX >= scaleZ) {
            basis0 = -basis0;
        } else if (scaleY >= scaleZ) {
            basis1 = -basis1;
        } else {
            basis2 = -basis2;
        }
    }

    rotation = glm::normalize(glm::quat_cast(glm::mat3{basis0, basis1, basis2}));
    return true;
}
} // namespace

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

    const CameraComponent* cameraComponent = nullptr;
    Entity cameraEntity;
    const auto cameraView = scene.getRegistry().view<const TransformComponent, const CameraComponent>();
    for (const auto entity : cameraView) {
        const auto& candidate = cameraView.get<const CameraComponent>(entity);
        if (!candidate.primary && cameraComponent != nullptr) {
            continue;
        }

        cameraComponent = &candidate;
        cameraEntity = Entity{entity};
        if (candidate.primary) {
            break;
        }
    }

    if (cameraComponent == nullptr || !cameraEntity) {
        return;
    }

    const float aspectRatio = static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height);
    const auto cameraWorld = scene.getWorldTransform(cameraEntity);
    glm::vec3 cameraPosition{cameraWorld[3]};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
    if (decomposePose(cameraWorld, cameraPosition, cameraRotation)) {
        const auto cameraNoScale = glm::translate(glm::mat4{1.0f}, cameraPosition) * glm::mat4_cast(cameraRotation);
        const auto view = glm::inverse(cameraNoScale);
        const auto projection = cameraComponent->camera.getProjection(aspectRatio);
        renderScene(scene, view, projection, cameraPosition, cameraComponent->camera.getExposure());
        return;
    }

    const auto view = glm::inverse(cameraWorld);
    const auto projection = cameraComponent->camera.getProjection(aspectRatio);
    renderScene(scene, view, projection, cameraPosition, cameraComponent->camera.getExposure());
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
        const auto lightView = scene.getRegistry().view<const TransformComponent, const DirectionalLightComponent>();
        for (const auto entity : lightView) {
            const auto& light = lightView.get<const DirectionalLightComponent>(entity);
            const auto world = scene.getWorldTransform(Entity{entity});
            glm::vec3 translation{0.0f};
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            if (decomposePose(world, translation, rotation)) {
                lighting.directional_light_count = 1;
                lighting.directional_light.direction = directionalLightDirection(rotation);
                lighting.directional_light.radiance = light.color * light.intensity;
                break;
            }

            lighting.directional_light_count = 1;
            lighting.directional_light.direction = directionalLightDirection(
                lightView.get<const TransformComponent>(entity).rotation);
            lighting.directional_light.radiance = light.color * light.intensity;
            break;
        }
    }

    m_renderer->setLighting(lighting);
    m_renderer->setViewProjection(view, projection, cameraPosition, exposure);

    const auto modelView = scene.getRegistry().view<const TransformComponent, const ModelComponent>();
    for (const auto entity : modelView) {
        const auto& modelComponent = modelView.get<const ModelComponent>(entity);
        if (!modelComponent.model.isValid()) {
            continue;
        }

        const auto* model = asset::AssetDatabase::get().get<renderer::interface::Model>(modelComponent.model);
        if (model == nullptr) {
            LUNA_CORE_ERROR("Entity {} has a null model asset", static_cast<uint32_t>(entity));
            continue;
        }

        m_renderer->renderModel(*model, scene.getWorldTransform(Entity{entity}));
    }
}
} // namespace lunalite::scene
