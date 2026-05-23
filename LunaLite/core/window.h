#pragma once
#include "../rhi/interface/rhi_types.h"

#include <cstdint>

#include <GLFW/glfw3.h>
#include <memory>
#include <string>

namespace lunalite::core {

class WindowCreateInfo {
public:
    uint32_t width{800};
    uint32_t height{600};
    std::string title{"LunaLite"};

    rhi::WindowRequirements requirements;
};

struct GLFWWindowDeleter {
    void operator()(GLFWwindow* window) const noexcept
    {
        if (window) {
            glfwDestroyWindow(window);
        }
    }
};

class Window {
public:
    explicit Window(const WindowCreateInfo& info);
    ~Window();

    void init();
    void onUpdate();

    rhi::WindowHandle getRHIWindowHandle();

    bool shouldClose();

private:
    std::unique_ptr<GLFWwindow, GLFWWindowDeleter> m_window{nullptr};
    WindowCreateInfo m_info;
};

} // namespace lunalite::core
