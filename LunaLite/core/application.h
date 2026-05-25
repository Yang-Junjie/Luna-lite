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

namespace lunalite::rhi {
class Instance;
}

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

private:
    void initialize(const ApplicationCreateInfo& info);
    void shutdown();
    void onEvent(Event& event);
    bool onWindowClose(WindowCloseEvent& event);
    bool onWindowResize(WindowResizeEvent& event);

private:
    bool m_is_running{true};

    std::unique_ptr<rhi::Instance> m_instance;
    std::unique_ptr<Window> m_window;

    std::unique_ptr<renderer::RendererController> m_renderer_controller;
    std::unique_ptr<renderer::RHIFramePresenter> m_frame_presenter;
    std::unique_ptr<scene::SceneRenderer> m_scene_renderer;

    LayerStack m_layer_stack;
};

Application* createApplication(int argc, char** argv);
} // namespace lunalite::core
