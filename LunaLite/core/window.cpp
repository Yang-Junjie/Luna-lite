#include "window.h"

namespace lunalite::core {
namespace {
bool glfwMakeCurrent(void* user_data)
{
    auto* window = static_cast<GLFWwindow*>(user_data);
    if (window == nullptr) {
        return false;
    }

    glfwMakeContextCurrent(window);
    return true;
}

void* glfwLoadProc(void*, const char* name)
{
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

void glfwSwapWindowBuffers(void* user_data)
{
    auto* window = static_cast<GLFWwindow*>(user_data);
    if (window != nullptr) {
        glfwSwapBuffers(window);
    }
}

void glfwSetWindowSwapInterval(void*, int interval)
{
    glfwSwapInterval(interval);
}

void glfwGetWindowFramebufferSize(void* user_data, uint32_t& width, uint32_t& height)
{
    auto* window = static_cast<GLFWwindow*>(user_data);
    if (window == nullptr) {
        width = 0;
        height = 0;
        return;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    width = static_cast<uint32_t>(framebuffer_width);
    height = static_cast<uint32_t>(framebuffer_height);
}
} // namespace

Window::Window(const WindowCreateInfo& info)
    : m_info(info)
{
    init();

    glfwDefaultWindowHints();
    if (m_info.requirements.backend == rhi::BackendType::OpenGL) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_info.requirements.glMajor);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_info.requirements.glMinor);

        if (m_info.requirements.gl_core_profile) {
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }

        if (m_info.requirements.gl_debug_context) {
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        }
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    m_window.reset(glfwCreateWindow(m_info.width, m_info.height, m_info.title.c_str(), nullptr, nullptr));
    m_width = m_info.width;
    m_height = m_info.height;

    m_surface_desc.backend = m_info.requirements.backend;
    if (m_info.requirements.backend == rhi::BackendType::OpenGL) {
        m_surface_desc.kind = rhi::SurfaceKind::OpenGLContext;
        m_surface_desc.opengl = rhi::OpenGLSurfaceCallbacks{
            .user_data = m_window.get(),
            .make_current = glfwMakeCurrent,
            .get_proc_address = glfwLoadProc,
            .swap_buffers = glfwSwapWindowBuffers,
            .set_swap_interval = glfwSetWindowSwapInterval,
            .get_framebuffer_size = glfwGetWindowFramebufferSize,
        };
    } else {
        m_surface_desc.kind = rhi::SurfaceKind::NativeWindow;
    }
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

const rhi::SurfaceDesc& Window::getSurfaceDesc() const
{
    return m_surface_desc;
}

uint32_t Window::getWidth() const
{
    return m_width;
}

uint32_t Window::getHeight() const
{
    return m_height;
}

void Window::resize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}
} // namespace lunalite::core
