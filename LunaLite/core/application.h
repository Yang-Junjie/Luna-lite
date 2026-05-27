#pragma once
#include "../renderer/interface/renderer_kind.h"
#include "application_event.h"
#include "event.h"
#include "layer_stack.h"
#include "TinyRHI/interface/rhi_types.h"
#include "window.h"

#include <cstdint>

#include <memory>
#include <string>

namespace lunalite::renderer {
class RHIFramePresenter;
class RendererController;
} // namespace lunalite::renderer

namespace lunalite::renderer::interface {
struct FrameImage;
}

namespace lunalite::imgui {
class ImGuiRenderer;
class ImGuiPlatform;
} // namespace lunalite::imgui

namespace lunalite::rhi {
class Device;
class Instance;
class Swapchain;
} // namespace lunalite::rhi

namespace lunalite::scene {
class Scene;
class SceneRenderer;
} // namespace lunalite::scene

namespace lunalite::core {

struct ApplicationCreateInfo {
    std::string name{"LunaLite"};
    uint32_t width{1'280};
    uint32_t height{720};
    rhi::BackendType backend{rhi::BackendType::OpenGL};
    renderer::interface::RendererKind renderer_kind{renderer::interface::RendererKind::Default};
    bool enable_imgui{false};
    bool enable_imgui_viewports{true};
    bool present_scene_to_swapchain{true};
};

class Application {
public:
    explicit Application(const ApplicationCreateInfo& info);
    ~Application();

    static Application& get();

    void run();
    void close();
    void pushLayer(std::unique_ptr<Layer> layer);
    void pushOverlay(std::unique_ptr<Layer> overlay);
    void switchRenderer(renderer::interface::RendererKind kind);

    scene::SceneRenderer& getSceneRenderer();
    imgui::ImGuiRenderer& getImGuiRenderer();
    const renderer::interface::FrameImage& getFrameImage() const;

private:
    void initialize(const ApplicationCreateInfo& info);
    void initializeImGui(const ApplicationCreateInfo& info);
    void shutdown();
    void shutdownImGui();
    void onEvent(Event& event);
    bool onWindowClose(WindowCloseEvent& event);
    bool onWindowResize(WindowResizeEvent& event);

private:
    bool m_is_running{true};

    std::unique_ptr<rhi::Instance> m_instance;
    std::unique_ptr<Window> m_window;
    rhi::Device* m_device{nullptr};
    rhi::SurfaceHandle m_surface_handle{};
    rhi::SwapchainHandle m_swapchain_handle{};
    rhi::Swapchain* m_swapchain{nullptr};

    std::unique_ptr<renderer::RendererController> m_renderer_controller;
    std::unique_ptr<renderer::RHIFramePresenter> m_frame_presenter;
    std::unique_ptr<scene::SceneRenderer> m_scene_renderer;
    std::unique_ptr<imgui::ImGuiPlatform> m_imgui_platform;
    std::unique_ptr<imgui::ImGuiRenderer> m_imgui_renderer;
    bool m_present_scene_to_swapchain{true};

    LayerStack m_layer_stack;
};

Application* createApplication(int argc, char** argv);
} // namespace lunalite::core
