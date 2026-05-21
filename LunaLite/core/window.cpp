#include "window.h"

namespace lunalite::core {
Window::Window(const WindowCreateInfo& info)
    : m_info(info)
{
    init();

    glfwDefaultWindowHints();
    if (m_info.requirements.backend == rhi::BackendType::OpenGL) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_info.requirements.glMajor);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_info.requirements.glMinor);

        if (m_info.requirements.glCoreProfile) {
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }

        if (m_info.requirements.glDebugContext) {
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        }
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    m_window.reset(glfwCreateWindow(m_info.width, m_info.height, m_info.title.c_str(), nullptr, nullptr));
}

Window::~Window()
{
    m_window.reset();
    glfwTerminate();
}

void Window::onUpdate()
{
    glfwPollEvents();
}

void Window::init()
{
    glfwInit();
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(m_window.get());
}

rhi::WindowHandle Window::getRHIWindowHandle()
{
    return rhi::WindowHandle{m_window.get()};
}
} // namespace lunalite::core
