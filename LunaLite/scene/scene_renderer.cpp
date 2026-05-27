#include "../asset/asset_database.h"
#include "../core/log.h"
#include "../renderer/interface/mesh.h"
#include "../renderer/interface/renderer.h"
#include "components.h"
#include "scene_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lunalite::scene {

SceneRenderer::SceneRenderer(renderer::interface::Renderer& renderer)
    : m_renderer(&renderer)
{}

void SceneRenderer::setRenderer(renderer::interface::Renderer& renderer)
{
    m_renderer = &renderer;
    LUNA_CORE_DEBUG("Scene renderer target changed");
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

void SceneRenderer::render(const Scene& scene)
{
    LUNA_ASSERT(m_renderer, "SceneRenderer has no renderer.");

    // Collect first directional light
    {
        const auto lightView = scene.getRegistry().view<const DirectionalLightComponent>();
        if (!lightView.empty()) {
            const auto& light = lightView.get<const DirectionalLightComponent>(*lightView.begin());
            m_renderer->setDirectionalLight(light.direction, light.ambient, light.diffuse, light.specular);
        }
    }

    // Set default camera
    {
        const auto cameraPos = glm::vec3(3.0f, 2.0f, 5.0f);
        const auto target = glm::vec3(0.0f, 0.0f, 0.0f);
        const auto up = glm::vec3(0.0f, 1.0f, 0.0f);
        const auto view = glm::lookAt(cameraPos, target, up);
        const auto proj = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

        m_renderer->setViewProjection(view, proj, cameraPos);
    }

    const auto view = scene.getRegistry().view<const TransformComponent, const MeshComponent>();
    for (const auto entity : view) {
        const auto& transform = view.get<const TransformComponent>(entity);
        const auto& meshComponent = view.get<const MeshComponent>(entity);
        const auto* mesh = asset::AssetDatabase::get().get<renderer::interface::Mesh>(meshComponent.mesh);
        if (mesh == nullptr) {
            LUNA_CORE_ERROR("Entity {} has a null mesh asset", static_cast<uint32_t>(entity));
            continue;
        }

        m_renderer->renderMesh(*mesh, transform.getTransform());
    }
}
} // namespace lunalite::scene
