#pragma once
#include "layer_stack.h"
#include "window.h"
#include "../rhi/interface/rhi_types.h"

#include <cstdint>
#include <memory>
#include <string>

namespace lunalite::renderer {
class Renderer;
}

namespace lunalite::rhi {
class Instance;
}

namespace lunalite::scene {
class Scene;
class SceneRenderer;
}

namespace lunalite::core {

struct ApplicationCreateInfo {
    std::string name{"LunaLite"};
    uint32_t width{1280};
    uint32_t height{720};
    rhi::BackendType backend{rhi::BackendType::OpenGL};
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

    scene::SceneRenderer& getSceneRenderer();

private:
    void initialize(const ApplicationCreateInfo& info);
    void shutdown();

private:
    bool m_is_running{true};

    std::unique_ptr<rhi::Instance> m_rhi;
    std::unique_ptr<Window> m_window;

    std::unique_ptr<renderer::Renderer> m_renderer;
    std::unique_ptr<scene::SceneRenderer> m_scene_renderer;

    LayerStack m_layer_stack;
};

Application* createApplication(int argc, char** argv);
} // namespace lunalite::core
