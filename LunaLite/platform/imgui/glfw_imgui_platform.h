#pragma once

#include "../../imgui/imgui_platform.h"

struct GLFWwindow;

namespace lunalite::platform {

class GLFWImGuiPlatform final : public imgui::ImGuiPlatform {
public:
    GLFWImGuiPlatform() = default;
    ~GLFWImGuiPlatform() override;

    GLFWImGuiPlatform(const GLFWImGuiPlatform&) = delete;
    GLFWImGuiPlatform& operator=(const GLFWImGuiPlatform&) = delete;

    bool init(core::Window& window) override;
    void shutdown() override;
    void newFrame() override;
    rhi::NativeWindowHandle nativeWindowHandle(const ImGuiViewport& viewport) const override;

private:
    GLFWwindow* m_window{nullptr};
};

} // namespace lunalite::platform
