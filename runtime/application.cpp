#include "../LunaLite/core/application.h"
#include "../LunaLite/modules/default_engine_modules.h"
#include "demo_layer.h"

#include <filesystem>
#include <memory>

namespace lunalite::core {
namespace {
std::filesystem::path getExecutableDirectory(int argc, char** argv)
{
    if (argc <= 0 || argv == nullptr || argv[0] == nullptr || argv[0][0] == '\0') {
        return std::filesystem::current_path();
    }

    const auto executable_path = std::filesystem::absolute(argv[0]);
    if (executable_path.has_parent_path()) {
        return executable_path.parent_path();
    }

    return std::filesystem::current_path();
}
} // namespace

Application* createApplication(int argc, char** argv)
{
    modules::registerDefaultEngineModules();

    ApplicationCreateInfo info;
    info.name = "LunaLite";
    info.width = 1'280;
    info.height = 720;
    info.backend = rhi::BackendType::OpenGL;
    info.renderer_kind = renderer::interface::RendererKind::Default;
    info.enable_imgui = false;
    info.enable_imgui_viewports = false;
    info.present_scene_to_swapchain = true;

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<runtime::DemoLayer>(getExecutableDirectory(argc, argv)));

    return app;
}
} // namespace lunalite::core
