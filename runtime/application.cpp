#include "../LunaLite/core/application.h"
#include "demo_layer.h"

#include <memory>

namespace lunalite::core {
Application* createApplication(int argc, char** argv)
{
    ApplicationCreateInfo info;
    info.name = "LunaLite";
    info.width = 1'280;
    info.height = 720;
    info.backend = rhi::BackendType::OpenGL;

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<runtime::DemoLayer>());

    return app;
}
} // namespace lunalite::core
