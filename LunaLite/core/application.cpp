#include "application.h"

#include "../renderer/controller/renderer_controller.h"
#include "../renderer/frame_presenter/rhi_frame_presenter.h"
#include "../rhi/backend_factory.h"
#include "../scene/scene_renderer.h"
#include "layer.h"

#include <chrono>
#include <stdexcept>

namespace lunalite::core {
namespace {
Application* s_instance = nullptr;
}

Application::Application(const ApplicationCreateInfo& info)
    : m_rhi(rhi::BackendFactory::createInstance(info.backend))
{
    if (s_instance != nullptr) {
        throw std::runtime_error("Application already exists.");
    }

    s_instance = this;
    initialize(info);
}

Application::~Application()
{
    shutdown();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

Application& Application::get()
{
    if (s_instance == nullptr) {
        throw std::runtime_error("Application has not been created.");
    }

    return *s_instance;
}

void Application::run()
{
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (m_is_running && !m_window->shouldClose()) {
        const auto currentFrameTime = std::chrono::steady_clock::now();
        const std::chrono::duration<float> deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        m_window->onUpdate();

        for (auto& layer : m_layer_stack) {
            layer->onUpdate(deltaTime.count());
        }

        m_scene_renderer->beginFrame();

        for (auto& layer : m_layer_stack) {
            layer->onRender();
        }

        m_scene_renderer->endFrame();
        m_frame_presenter->present(m_renderer_controller->getFrameImage());
    }
}

void Application::close()
{
    m_is_running = false;
}

void Application::pushLayer(std::unique_ptr<Layer> layer)
{
    m_layer_stack.pushLayer(std::move(layer));
}

void Application::pushOverlay(std::unique_ptr<Layer> overlay)
{
    m_layer_stack.pushOverlay(std::move(overlay));
}

void Application::switchRenderer(renderer::interface::RendererKind kind)
{
    m_renderer_controller->switchRenderer(kind);
    m_scene_renderer->setRenderer(m_renderer_controller->getRenderer());
}

scene::SceneRenderer& Application::getSceneRenderer()
{
    return *m_scene_renderer;
}

void Application::initialize(const ApplicationCreateInfo& info)
{
    if (!m_rhi) {
        throw std::runtime_error("Failed to create RHI instance.");
    }

    WindowCreateInfo window_info;
    window_info.width = info.width;
    window_info.height = info.height;
    window_info.title = info.name;
    window_info.requirements = m_rhi->getWindowRequirements();

    m_window = std::make_unique<Window>(window_info);

    if (!m_rhi->init(m_window->getRHIWindowHandle())) {
        throw std::runtime_error("Failed to initialize RHI instance.");
    }

    m_renderer_controller =
        std::make_unique<renderer::RendererController>(*m_rhi, info.width, info.height, info.renderer_kind);
    m_frame_presenter = std::make_unique<renderer::RHIFramePresenter>(*m_rhi->getDevice(), *m_rhi->getSwapchain());
    m_scene_renderer.reset(new scene::SceneRenderer(m_renderer_controller->getRenderer()));
}

void Application::shutdown()
{
    m_scene_renderer.reset();
    m_frame_presenter.reset();
    m_renderer_controller.reset();

    if (m_rhi) {
        m_rhi->shutdown();
    }
}
} // namespace lunalite::core
