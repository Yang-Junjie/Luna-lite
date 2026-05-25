#pragma once
#include "../../core/input.h"
#include "../../core/window.h"

#include <GLFW/glfw3.h>

#include <memory>

namespace lunalite::platform {

struct GLFWWindowDeleter {
    void operator()(GLFWwindow* window) const noexcept
    {
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
    }
};

class GLFWWindow final : public core::Window, public core::InputProvider {
public:
    explicit GLFWWindow(const core::WindowCreateInfo& info);
    ~GLFWWindow() override;

    void init() override;
    void onUpdate() override;
    void setEventCallback(const EventCallbackFn& callback) override;
    bool shouldClose() override;

    const rhi::SurfaceDesc& getSurfaceDesc() const override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    void resize(uint32_t width, uint32_t height) override;

    bool isKeyPressed(core::KeyCode key) const override;
    bool isMouseButtonPressed(core::MouseCode button) const override;
    glm::vec2 getMousePosition() const override;
    core::CursorMode getCursorMode() const override;
    void setCursorMode(core::CursorMode mode) override;
    void setMousePosition(float x, float y) override;
    void setRawMouseMotion(bool enabled) override;

    void dispatchEvent(core::Event& event);

private:
    std::unique_ptr<GLFWwindow, GLFWWindowDeleter> m_window{nullptr};
    EventCallbackFn m_event_callback;
    core::WindowCreateInfo m_info;
    rhi::SurfaceDesc m_surface_desc{};
    uint32_t m_width{0};
    uint32_t m_height{0};
};

} // namespace lunalite::platform
