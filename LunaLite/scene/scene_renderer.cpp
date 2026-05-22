#include "scene_renderer.h"

namespace lunalite::scene {

void Scene::addTriangle(renderer::interface::Triangle triangle)
{
    m_triangles.push_back(triangle);
}

SceneRenderer::SceneRenderer(renderer::interface::Renderer& renderer)
    : m_renderer(renderer)
{}

void SceneRenderer::render(const Scene& scene)
{
    m_renderer.beginFrame();

    for (const auto& triangle : scene.getTriangles()) {
        m_renderer.renderTriangle(triangle);
    }

    m_renderer.endFrame();
}
} // namespace lunalite::scene
