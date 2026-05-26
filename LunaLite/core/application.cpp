#include "../renderer/controller/renderer_controller.h"
#include "../renderer/frame_presenter/rhi_frame_presenter.h"
#include "../scene/scene_renderer.h"
#include "application.h"
#include "application_event.h"
#include "layer.h"
#include "log.h"
#include "TinyRHI/backend_factory.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"

#include <chrono>
#include <stdexcept>

namespace lunalite::core {
namespace {
Application* s_instance = nullptr;
}

Application::Application(const ApplicationCreateInfo& info)
    : m_instance(rhi::BackendFactory::createInstance(info.backend))
{
    LUNA_ASSERT(s_instance == nullptr, "Application already exists.");

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
    LUNA_ASSERT(s_instance != nullptr, "Application instance is null.");

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
    LUNA_ASSERT(m_instance, "Failed to create RHI instance.");

    WindowCreateInfo window_info;
    window_info.width = info.width;
    window_info.height = info.height;
    window_info.title = info.name;

    m_window = Window::create(window_info);
    m_window->setEventCallback([this](Event& event) {
        onEvent(event);
    });

    if (!m_instance->init()) {
        LUNA_CORE_FATAL("Failed to initialize RHI instance.");
    }

    m_device = m_instance->getDevice();
    LUNA_ASSERT(m_device != nullptr, "RHI instance returned null device.");

    m_swapchain_handle = m_device->createSwapchain(*m_window, rhi::SwapchainDesc{});
    m_swapchain = m_device->getSwapchain(m_swapchain_handle);
    if (m_swapchain == nullptr) {
        LUNA_CORE_FATAL("Failed to create RHI swapchain.");
    }

    m_renderer_controller = std::make_unique<renderer::RendererController>(
        *m_device, *m_swapchain, info.width, info.height, info.renderer_kind);
    m_frame_presenter = std::make_unique<renderer::RHIFramePresenter>(*m_device, *m_swapchain);
    m_scene_renderer.reset(new scene::SceneRenderer(m_renderer_controller->getRenderer()));
}

void Application::shutdown()
{
    m_scene_renderer.reset();
    m_frame_presenter.reset();
    m_renderer_controller.reset();

    if (m_device != nullptr && m_swapchain_handle != 0) {
        m_device->destroySwapchain(m_swapchain_handle);
        m_swapchain_handle = 0;
        m_swapchain = nullptr;
    }

    if (m_instance) {
        m_instance->shutdown();
    }
    m_device = nullptr;
    m_window.reset();
}

void Application::onEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
        return onWindowClose(e);
    });
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
        return onWindowResize(e);
    });

    for (auto it = m_layer_stack.rbegin(); it != m_layer_stack.rend(); ++it) {
        if (event.m_handled) {
            break;
        }

        (*it)->onEvent(event);
    }
}

bool Application::onWindowClose(WindowCloseEvent&)
{
    close();
    return true;
}

bool Application::onWindowResize(WindowResizeEvent& event)
{
    m_renderer_controller->resize(event.getWidth(), event.getHeight());
    return false;
}
} // namespace lunalite::core
