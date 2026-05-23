#include "swapchain.h"

#include "device.h"

#include <GLFW/glfw3.h>

namespace lunalite::rhi {
OpenGLSwapchain::OpenGLSwapchain(OpenGLDevice& device, GLFWwindow* window)
    : m_device(device),
      m_window(window),
      m_color_view(m_device.createSwapchainTextureView(TextureFormat::RGBA8)),
      m_depth_stencil_view(m_device.createSwapchainTextureView(TextureFormat::Depth24Stencil8))
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

TextureViewHandle OpenGLSwapchain::getCurrentColorTextureView() const
{
    return m_color_view;
}

TextureViewHandle OpenGLSwapchain::getDepthStencilTextureView() const
{
    return m_depth_stencil_view;
}

uint32_t OpenGLSwapchain::getWidth() const
{
    return m_width;
}

uint32_t OpenGLSwapchain::getHeight() const
{
    return m_height;
}

void OpenGLSwapchain::resize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}

void OpenGLSwapchain::present()
{
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}
} // namespace lunalite::rhi
