#include "device.h"
#include "instance.h"
#include "swapchain.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace lunalite::rhi {

OpenGLInstance::~OpenGLInstance() = default;


bool OpenGLInstance::init(WindowHandle window)
{
    m_native_window = window.native_window;

    auto* glfwWindow = static_cast<GLFWwindow*>(m_native_window);
    if (glfwWindow == nullptr) {
        return false;
    }

    glfwMakeContextCurrent(glfwWindow);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        return false;
    }

    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);

    auto device = std::make_unique<OpenGLDevice>();
    m_swapchain = std::make_unique<OpenGLSwapchain>(*device, glfwWindow);
    m_device = std::move(device);
    return true;
}

void OpenGLInstance::shutdown()
{
    m_swapchain.reset();
    m_device.reset();
    m_native_window = nullptr;
}

void OpenGLInstance::resize(uint32_t width, uint32_t height)
{
    glViewport(0, 0, static_cast<int>(width), static_cast<int>(height));
    if (m_swapchain) {
        m_swapchain->resize(width, height);
    }
}

Device* OpenGLInstance::getDevice()
{
    return m_device.get();
}

Swapchain* OpenGLInstance::getSwapchain()
{
    return m_swapchain.get();
}

} // namespace lunalite::rhi
