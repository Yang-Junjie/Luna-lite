#include "../imgui/imgui_platform.h"
#include "../imgui/imgui_renderer.h"
#include "../renderer/debug_renderer.h"
#include "../renderer/default_renderer/renderer.h"
#include "../renderer/frame_presenter/rhi_frame_presenter.h"
#include "../scene/scene_renderer.h"
#include "application.h"
#include "application_event.h"
#include "layer.h"
#include "log.h"
#include "TinyRHI/backend_factory.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/instance.h"
#include "TinyRHI/interface/surface.h"
#include "TinyRHI/interface/swapchain.h"

#include <chrono>
#include <imgui.h>
#include <stdexcept>

namespace lunalite::core {
namespace {
Application* s_instance = nullptr;

const char* backendName(rhi::BackendType backend)
{
    switch (backend) {
        case rhi::BackendType::OpenGL:
            return "OpenGL";
        case rhi::BackendType::Vulkan:
            return "Vulkan";
        case rhi::BackendType::D3D12:
            return "D3D12";
        case rhi::BackendType::Metal:
            return "Metal";
    }

    return "Unknown";
}
} // namespace

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
    LUNA_ASSERT(m_window, "Application window is null.");
    LUNA_ASSERT(m_renderer, "Renderer is null.");
    LUNA_ASSERT(m_frame_presenter, "Frame presenter is null.");
    LUNA_ASSERT(m_scene_renderer, "Scene renderer is null.");
    LUNA_ASSERT(m_debug_renderer, "Debug renderer is null.");

    LUNA_CORE_INFO("Application main loop started");
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (m_is_running && !m_window->shouldClose()) {
        const auto currentFrameTime = std::chrono::steady_clock::now();
        const std::chrono::duration<float> deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        m_stats.updateFrame(deltaTime.count());

        m_frame_render_data.clear();
        m_scene_renderer->beginFrame(m_frame_render_data);
        m_debug_renderer->beginFrame(m_frame_render_data);

        m_window->onUpdate();

        for (auto& layer : m_layer_stack) {
            layer->onUpdate(deltaTime.count());
        }

        for (auto& layer : m_layer_stack) {
            layer->onRender();
        }

        m_debug_renderer->endFrame();
        m_scene_renderer->endFrame();

        m_renderer->beginFrame();
        m_renderer->renderFrame(m_frame_render_data);
        m_renderer->endFrame();
        m_stats.setRenderStats(m_renderer->getStats());

        if (m_imgui_renderer) {
            rhi::SwapchainFrame frame{};
            if (!m_device->beginFrame(m_swapchain_handle, frame)) {
                break;
            }

            if (m_present_scene_to_swapchain) {
                m_frame_presenter->renderToSwapchain(m_renderer->getFrameImage(), frame);
            }

            m_imgui_renderer->beginFrame();

            for (auto& layer : m_layer_stack) {
                layer->onImGuiRender();
            }

            m_imgui_renderer->endFrame(m_present_scene_to_swapchain ? imgui::ImGuiRenderMode::OverlaySwapchain
                                                                    : imgui::ImGuiRenderMode::ClearSwapchain,
                                       frame);
            m_stats.setImGuiStats(m_imgui_renderer->getStats());
        } else {
            m_stats.clearImGuiStats();
            m_frame_presenter->present(m_renderer->getFrameImage());
        }
    }

    LUNA_CORE_INFO("Application main loop finished");
}

void Application::close()
{
    LUNA_CORE_DEBUG("Application close requested");
    m_is_running = false;
}

void Application::pushLayer(std::unique_ptr<Layer> layer)
{
    LUNA_ASSERT(layer, "Cannot push a null layer.");
    m_layer_stack.pushLayer(std::move(layer));
}

void Application::pushOverlay(std::unique_ptr<Layer> overlay)
{
    LUNA_ASSERT(overlay, "Cannot push a null overlay.");
    m_layer_stack.pushOverlay(std::move(overlay));
}

scene::SceneRenderer& Application::getSceneRenderer()
{
    LUNA_ASSERT(m_scene_renderer, "Scene renderer is null.");
    return *m_scene_renderer;
}

renderer::DebugRenderer& Application::getDebugRenderer()
{
    LUNA_ASSERT(m_debug_renderer, "Debug renderer is null.");
    return *m_debug_renderer;
}

const renderer::interface::FrameImage& Application::getFrameImage() const
{
    LUNA_ASSERT(m_renderer, "Renderer is null.");
    return m_renderer->getFrameImage();
}

imgui::ImGuiRenderer& Application::getImGuiRenderer()
{
    LUNA_ASSERT(m_imgui_renderer, "ImGui renderer is null.");
    return *m_imgui_renderer;
}

const diagnostics::RuntimeStats& Application::getStats() const
{
    return m_stats;
}

void Application::initialize(const ApplicationCreateInfo& info)
{
    LUNA_ASSERT(m_instance, "Failed to create RHI instance.");
    LUNA_ASSERT(info.width > 0 && info.height > 0, "Application size must be non-zero.");
    LUNA_CORE_INFO("Initializing application '{}' ({}x{}, backend: {})",
                   info.name,
                   info.width,
                   info.height,
                   backendName(info.backend));
    m_present_scene_to_swapchain = info.present_scene_to_swapchain;

    WindowCreateInfo window_info;
    window_info.width = info.width;
    window_info.height = info.height;
    window_info.title = info.name;

    m_window = Window::create(window_info);
    LUNA_ASSERT(m_window, "Failed to create window.");
    m_window->setEventCallback([this](Event& event) {
        onEvent(event);
    });

    if (!m_instance->init()) {
        LUNA_CORE_FATAL("Failed to initialize RHI instance.");
    }

    m_device = m_instance->getDevice();
    LUNA_ASSERT(m_device != nullptr, "RHI instance returned null device.");
    LUNA_CORE_INFO("RHI instance initialized");

    m_surface_handle = m_instance->createSurface(m_window->getNativeHandle());
    if (!m_surface_handle) {
        LUNA_CORE_FATAL("Failed to create RHI surface.");
    }
    LUNA_CORE_INFO("RHI surface created");

    if (auto* surface = m_instance->getSurface(m_surface_handle)) {
        surface->resize(info.width, info.height);
    } else {
        LUNA_CORE_FATAL("Failed to resolve RHI surface.");
    }

    m_swapchain_handle = m_device->createSwapchain(m_surface_handle, rhi::SwapchainDesc{});
    m_swapchain = m_device->getSwapchain(m_swapchain_handle);
    if (m_swapchain == nullptr) {
        LUNA_CORE_FATAL("Failed to create RHI swapchain.");
    }
    LUNA_CORE_INFO("RHI swapchain created ({}x{})", m_swapchain->getWidth(), m_swapchain->getHeight());

    m_renderer = std::make_unique<renderer::Renderer>(*m_device, *m_swapchain);
    m_renderer->resize(info.width, info.height);
    m_frame_presenter = std::make_unique<renderer::RHIFramePresenter>(*m_device, m_swapchain_handle);
    m_debug_renderer = std::make_unique<renderer::DebugRenderer>();
    m_scene_renderer.reset(new scene::SceneRenderer());
    m_scene_renderer->setViewportSize(info.width, info.height);

    if (info.enable_imgui) {
        initializeImGui(info);
    }

    LUNA_CORE_INFO("Application initialized");
}

void Application::initializeImGui(const ApplicationCreateInfo& info)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    if (info.enable_imgui_viewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    m_imgui_platform = imgui::ImGuiPlatform::create();
    if (!m_imgui_platform || !m_imgui_platform->init(*m_window)) {
        LUNA_CORE_FATAL("Failed to initialize ImGui platform backend.");
    }

    m_imgui_renderer = std::make_unique<imgui::ImGuiRenderer>();
    m_imgui_renderer->setSurfaceOwner(*m_instance);
    m_imgui_renderer->setPlatform(*m_imgui_platform);
    if (!m_imgui_renderer->init(*m_device, m_swapchain_handle, *m_swapchain)) {
        LUNA_CORE_FATAL("Failed to initialize ImGui renderer backend.");
    }

    LUNA_CORE_INFO("ImGui initialized");
}

void Application::shutdown()
{
    LUNA_CORE_INFO("Application shutdown started");

    shutdownImGui();
    m_debug_renderer.reset();
    m_scene_renderer.reset();
    m_frame_presenter.reset();
    m_renderer.reset();

    if (m_device != nullptr && m_swapchain_handle) {
        m_device->destroySwapchain(m_swapchain_handle);
        m_swapchain_handle = {};
        m_swapchain = nullptr;
    }

    if (m_instance && m_surface_handle) {
        m_instance->destroySurface(m_surface_handle);
        m_surface_handle = {};
    }

    if (m_instance) {
        m_instance->shutdown();
    }
    m_device = nullptr;
    m_window.reset();

    LUNA_CORE_INFO("Application shutdown finished");
}

void Application::shutdownImGui()
{
    if (m_imgui_renderer) {
        m_imgui_renderer->shutdown();
        m_imgui_renderer.reset();
    }

    if (m_imgui_platform) {
        m_imgui_platform->shutdown();
        m_imgui_platform.reset();
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }
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
    LUNA_CORE_INFO("Window close event received");
    close();
    return true;
}

bool Application::onWindowResize(WindowResizeEvent& event)
{
    LUNA_CORE_DEBUG("Window resized to {}x{}", event.getWidth(), event.getHeight());

    if (m_instance && m_surface_handle) {
        if (auto* surface = m_instance->getSurface(m_surface_handle)) {
            surface->resize(event.getWidth(), event.getHeight());
        }
    }

    LUNA_ASSERT(m_renderer, "Renderer is null.");
    if (m_swapchain) {
        m_swapchain->resize(event.getWidth(), event.getHeight());
    }
    m_renderer->resize(event.getWidth(), event.getHeight());
    if (m_scene_renderer) {
        m_scene_renderer->setViewportSize(event.getWidth(), event.getHeight());
    }
    return false;
}
} // namespace lunalite::core
