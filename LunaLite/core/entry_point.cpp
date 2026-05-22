#include "../renderer/default_renderer/renderer.h"
#include "../renderer/interface/triangle.h"
#include "../rhi/backend_factory.h"
#include "../rhi/opengl/instance.h"
#include "../scene/scene.h"
#include "../scene/scene_renderer.h"
#include "window.h"

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

    lunalite::scene::Scene scene;
    scene.addTriangle(lunalite::renderer::interface::Triangle{
        lunalite::renderer::interface::Vertex{
            .position = {0.0f, 0.5f, 0.0f},
            .normal = {0.0f, 0.0f, 1.0f},
            .uv = {0.0f, 0.0f},
            .color = {1.0f, 0.1f, 0.1f},
        },
        lunalite::renderer::interface::Vertex{
            .position = {-0.5f, -0.5f, 0.0f},
            .normal = {0.0f, 0.0f, 1.0f},
            .uv = {0.0f, 0.0f},
            .color = {0.1f, 1.0f, 0.1f},
        },
        lunalite::renderer::interface::Vertex{
            .position = {0.5f, -0.5f, 0.0f},
            .normal = {0.0f, 0.0f, 1.0f},
            .uv = {0.0f, 0.0f},
            .color = {0.1f, 0.3f, 1.0f},
        },
    });

    lunalite::renderer::Renderer renderer(*instance);
    lunalite::scene::SceneRenderer scene_renderer(renderer);

    while (!window.shouldClose()) {
        window.onUpdate();
        scene_renderer.render(scene);
    }

    instance->shutdown();
    return 0;
}
