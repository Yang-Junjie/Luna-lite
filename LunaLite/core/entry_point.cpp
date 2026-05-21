#include "../rhi/opengl/instance.h"
#include "window.h"

int main()
{
    lunalite::rhi::OpenGLInstance rhi;

    lunalite::core::WindowCreateInfo windowInfo;
    windowInfo.width = 1'280;
    windowInfo.height = 720;
    windowInfo.title = "LunaLite";
    windowInfo.requirements = rhi.getWindowRequirements();

    lunalite::core::Window window(windowInfo);

    if (!rhi.initialize(window.getRHIWindowHandle())) {
        return -1;
    }

    while (!window.shouldClose()) {
        window.onUpdate();
        rhi.present();
    }

    rhi.shutdown();
    return 0;
}
