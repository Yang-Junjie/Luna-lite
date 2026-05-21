#include "instance.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace lunalite::rhi {

bool OpenGLInstance::initialize(WindowHandle window)
{
    m_nativeWindow = window.nativeWindow;

    auto* glfwWindow = static_cast<GLFWwindow*>(m_nativeWindow);
    glfwMakeContextCurrent(glfwWindow);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        return false;
    }

    glfwSwapInterval(1);
    return true;
}

void OpenGLInstance::shutdown()
{
    m_nativeWindow = nullptr;
}

void OpenGLInstance::present()
{
    auto* glfwWindow = static_cast<GLFWwindow*>(m_nativeWindow);
    glfwSwapBuffers(glfwWindow);
}

void OpenGLInstance::resize(uint32_t width, uint32_t height)
{
    glViewport(0, 0, static_cast<int>(width), static_cast<int>(height));
}

} // namespace lunalite::rhi
