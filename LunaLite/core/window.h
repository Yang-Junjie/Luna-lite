#pragma once
#include "TinyRHI/interface/rhi_types.h"
#include "TinyRHI/interface/surface.h"

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

class Window final : public rhi::Surface {
public:
    explicit Window(const WindowCreateInfo& info);
    ~Window();

    void init();
    void onUpdate();

    const rhi::SurfaceDesc& getSurfaceDesc() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;

    bool shouldClose();

private:
    std::unique_ptr<GLFWwindow, GLFWWindowDeleter> m_window{nullptr};
    WindowCreateInfo m_info;
    rhi::SurfaceDesc m_surface_desc{};
    uint32_t m_width{0};
    uint32_t m_height{0};
};

} // namespace lunalite::core
