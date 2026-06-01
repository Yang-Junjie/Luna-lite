#include "../LunaLite/core/application.h"
#include "test_app_layer.h"

#include <memory>

namespace lunalite::core {

Application* createApplication(int argc, char** argv)
{
    static_cast<void>(argc);
    static_cast<void>(argv);

    ApplicationCreateInfo info;
    info.name = "LunaLite Test App";
    info.width = 1'280;
    info.height = 720;
    info.backend = rhi::BackendType::OpenGL;
    info.renderer_kind = renderer::interface::RendererKind::Default;
    info.enable_imgui = false;
    info.enable_imgui_viewports = false;
    info.present_scene_to_swapchain = true;

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<test_app::TestAppLayer>());

    return app;
}

} // namespace lunalite::core
