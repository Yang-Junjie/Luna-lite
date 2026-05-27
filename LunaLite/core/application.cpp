#include "../renderer/controller/renderer_controller.h"
#include "../renderer/frame_presenter/rhi_frame_presenter.h"
#include "../imgui/imgui_platform.h"
#include "../imgui/imgui_renderer.h"
#include "../scene/scene_renderer.h"
#include "application.h"
#include "application_event.h"
#include "layer.h"
#include "log.h"
#include "TinyRHI/backend_factory.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/instance.h"
#include "TinyRHI/interface/render_pass.h"
#include "TinyRHI/interface/surface.h"
#include "TinyRHI/interface/swapchain.h"

#include <imgui.h>

#include <chrono>
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
    LUNA_ASSERT(m_window, "Application window is null.");
    LUNA_ASSERT(m_renderer_controller, "Renderer controller is null.");
    LUNA_ASSERT(m_frame_presenter, "Frame presenter is null.");
    LUNA_ASSERT(m_scene_renderer, "Scene renderer is null.");

    LUNA_CORE_INFO("Application main loop started");
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

        if (m_imgui_renderer) {
            LUNA_ASSERT(m_imgui_platform, "ImGui platform is null.");

            m_imgui_platform->newFrame();
            ImGui::NewFrame();

            for (auto& layer : m_layer_stack) {
                layer->onImGuiRender();
            }

            ImGui::Render();

            rhi::RenderPassBeginInfo pass;
            pass.color_attachments.push_back(rhi::ColorAttachmentDesc{
                .view = m_swapchain->getCurrentColorTextureView(),
                .load_op = rhi::LoadOp::Clear,
                .store_op = rhi::StoreOp::Store,
                .clear_color = rhi::ClearColor{0.08f, 0.09f, 0.11f, 1.0f},
            });
            pass.width = m_swapchain->getWidth();
            pass.height = m_swapchain->getHeight();

            auto& commands = m_device->getCommandList();
            commands.begin();
            commands.beginRenderPass(pass);
            m_imgui_renderer->render(ImGui::GetDrawData(), commands);
            commands.endRenderPass();
            commands.end();

            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault(nullptr, m_imgui_renderer.get());
            }

            m_swapchain->present();
        } else {
            m_frame_presenter->present(m_renderer_controller->getFrameImage());
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

void Application::switchRenderer(renderer::interface::RendererKind kind)
{
    LUNA_ASSERT(m_renderer_controller, "Renderer controller is null.");
    LUNA_ASSERT(m_scene_renderer, "Scene renderer is null.");

    m_renderer_controller->switchRenderer(kind);
    m_scene_renderer->setRenderer(m_renderer_controller->getRenderer());
}

scene::SceneRenderer& Application::getSceneRenderer()
{
    LUNA_ASSERT(m_scene_renderer, "Scene renderer is null.");
    return *m_scene_renderer;
}

const renderer::interface::FrameImage& Application::getFrameImage() const
{
    LUNA_ASSERT(m_renderer_controller, "Renderer controller is null.");
    return m_renderer_controller->getFrameImage();
}

imgui::ImGuiRenderer& Application::getImGuiRenderer()
{
    LUNA_ASSERT(m_imgui_renderer, "ImGui renderer is null.");
    return *m_imgui_renderer;
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

    m_renderer_controller = std::make_unique<renderer::RendererController>(
        *m_device, *m_swapchain, info.width, info.height, info.renderer_kind);
    m_frame_presenter = std::make_unique<renderer::RHIFramePresenter>(*m_device, *m_swapchain);
    m_scene_renderer.reset(new scene::SceneRenderer(m_renderer_controller->getRenderer()));
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
    if (!m_imgui_renderer->init(*m_device)) {
        LUNA_CORE_FATAL("Failed to initialize ImGui renderer backend.");
    }

    LUNA_CORE_INFO("ImGui initialized");
}

void Application::shutdown()
{
    LUNA_CORE_INFO("Application shutdown started");

    shutdownImGui();
    m_scene_renderer.reset();
    m_frame_presenter.reset();
    m_renderer_controller.reset();

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

    LUNA_ASSERT(m_renderer_controller, "Renderer controller is null.");
    m_renderer_controller->resize(event.getWidth(), event.getHeight());
    if (m_scene_renderer) {
        m_scene_renderer->setViewportSize(event.getWidth(), event.getHeight());
    }
    return false;
}
} // namespace lunalite::core
