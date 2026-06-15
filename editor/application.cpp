#include "../LunaLite/core/application.h"
#include "../LunaLite/modules/default_engine_modules.h"
#include "editor_layer.h"

#include <memory>

namespace lunalite::core {

Application* createApplication(int argc, char** argv)
{
    static_cast<void>(argc);
    static_cast<void>(argv);

    modules::registerDefaultEngineModules();

    ApplicationCreateInfo info;
    info.name = "LunaLite Editor";
    info.width = 1'600;
    info.height = 900;
    info.backend = rhi::BackendType::OpenGL;
    info.renderer_kind = renderer::interface::RendererKind::Default;
    info.enable_imgui = true;
    info.enable_imgui_viewports = true;
    info.present_scene_to_swapchain = false;

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<editor::EditorLayer>());

    return app;
}

} // namespace lunalite::core
