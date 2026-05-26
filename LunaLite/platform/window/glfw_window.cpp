#include "glfw_window.h"

#include "../../core/application_event.h"
#include "../../core/key_event.h"
#include "../../core/log.h"
#include "../../core/mouse_event.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace lunalite::platform {
namespace {
bool s_glfw_initialized = false;

struct KeyMapping {
    core::KeyCode key_code;
    int glfw_key;
};

struct MouseMapping {
    core::MouseCode mouse_code;
    int glfw_button;
};

constexpr KeyMapping kKeyMappings[] = {
    {core::KeyCode::LeftShift, GLFW_KEY_LEFT_SHIFT},
    {core::KeyCode::RightShift, GLFW_KEY_RIGHT_SHIFT},
    {core::KeyCode::LeftControl, GLFW_KEY_LEFT_CONTROL},
    {core::KeyCode::RightControl, GLFW_KEY_RIGHT_CONTROL},
    {core::KeyCode::LeftAlt, GLFW_KEY_LEFT_ALT},
    {core::KeyCode::RightAlt, GLFW_KEY_RIGHT_ALT},
    {core::KeyCode::Space, GLFW_KEY_SPACE},
    {core::KeyCode::Enter, GLFW_KEY_ENTER},
    {core::KeyCode::Delete, GLFW_KEY_DELETE},
    {core::KeyCode::Escape, GLFW_KEY_ESCAPE},
    {core::KeyCode::Up, GLFW_KEY_UP},
    {core::KeyCode::Down, GLFW_KEY_DOWN},
    {core::KeyCode::Left, GLFW_KEY_LEFT},
    {core::KeyCode::Right, GLFW_KEY_RIGHT},
    {core::KeyCode::Backspace, GLFW_KEY_BACKSPACE},
    {core::KeyCode::A, GLFW_KEY_A},
    {core::KeyCode::B, GLFW_KEY_B},
    {core::KeyCode::C, GLFW_KEY_C},
    {core::KeyCode::D, GLFW_KEY_D},
    {core::KeyCode::E, GLFW_KEY_E},
    {core::KeyCode::F, GLFW_KEY_F},
    {core::KeyCode::G, GLFW_KEY_G},
    {core::KeyCode::H, GLFW_KEY_H},
    {core::KeyCode::I, GLFW_KEY_I},
    {core::KeyCode::J, GLFW_KEY_J},
    {core::KeyCode::K, GLFW_KEY_K},
    {core::KeyCode::L, GLFW_KEY_L},
    {core::KeyCode::M, GLFW_KEY_M},
    {core::KeyCode::N, GLFW_KEY_N},
    {core::KeyCode::O, GLFW_KEY_O},
    {core::KeyCode::P, GLFW_KEY_P},
    {core::KeyCode::Q, GLFW_KEY_Q},
    {core::KeyCode::R, GLFW_KEY_R},
    {core::KeyCode::S, GLFW_KEY_S},
    {core::KeyCode::T, GLFW_KEY_T},
    {core::KeyCode::U, GLFW_KEY_U},
    {core::KeyCode::V, GLFW_KEY_V},
    {core::KeyCode::W, GLFW_KEY_W},
    {core::KeyCode::X, GLFW_KEY_X},
    {core::KeyCode::Y, GLFW_KEY_Y},
    {core::KeyCode::Z, GLFW_KEY_Z},
};

constexpr MouseMapping kMouseMappings[] = {
    {core::MouseCode::Left, GLFW_MOUSE_BUTTON_LEFT},
    {core::MouseCode::Right, GLFW_MOUSE_BUTTON_RIGHT},
    {core::MouseCode::Middle, GLFW_MOUSE_BUTTON_MIDDLE},
    {core::MouseCode::XButton1, GLFW_MOUSE_BUTTON_4},
    {core::MouseCode::XButton2, GLFW_MOUSE_BUTTON_5},
};

int toGLFWKey(core::KeyCode key)
{
    for (const auto& mapping : kKeyMappings) {
        if (mapping.key_code == key) {
            return mapping.glfw_key;
        }
    }

    return GLFW_KEY_UNKNOWN;
}

core::KeyCode fromGLFWKey(int key)
{
    for (const auto& mapping : kKeyMappings) {
        if (mapping.glfw_key == key) {
            return mapping.key_code;
        }
    }

    return core::KeyCode::None;
}

int toGLFWMouseButton(core::MouseCode button)
{
    for (const auto& mapping : kMouseMappings) {
        if (mapping.mouse_code == button) {
            return mapping.glfw_button;
        }
    }

    return -1;
}

core::MouseCode fromGLFWMouseButton(int button)
{
    for (const auto& mapping : kMouseMappings) {
        if (mapping.glfw_button == button) {
            return mapping.mouse_code;
        }
    }

    return core::MouseCode::None;
}

int toGLFWCursorMode(core::CursorMode mode)
{
    switch (mode) {
    case core::CursorMode::Normal:
        return GLFW_CURSOR_NORMAL;
    case core::CursorMode::Hidden:
        return GLFW_CURSOR_HIDDEN;
    case core::CursorMode::Locked:
        return GLFW_CURSOR_DISABLED;
    }

    return GLFW_CURSOR_NORMAL;
}

core::CursorMode fromGLFWCursorMode(int mode)
{
    switch (mode) {
    case GLFW_CURSOR_HIDDEN:
        return core::CursorMode::Hidden;
    case GLFW_CURSOR_DISABLED:
        return core::CursorMode::Locked;
    case GLFW_CURSOR_NORMAL:
    default:
        return core::CursorMode::Normal;
    }
}

GLFWWindow* getWindow(GLFWwindow* window)
{
    return static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
}

void dispatch(GLFWwindow* native_window, core::Event& event)
{
    if (auto* window = getWindow(native_window)) {
        window->dispatchEvent(event);
    }
}

void windowCloseCallback(GLFWwindow* window)
{
    core::WindowCloseEvent event;
    dispatch(window, event);
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    if (auto* luna_window = getWindow(window)) {
        luna_window->resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }

    core::WindowResizeEvent event(static_cast<unsigned int>(width), static_cast<unsigned int>(height));
    dispatch(window, event);
}

void windowFocusCallback(GLFWwindow* window, int focused)
{
    if (focused == GLFW_TRUE) {
        core::WindowFocusEvent event;
        dispatch(window, event);
    } else {
        core::WindowLostFocusEvent event;
        dispatch(window, event);
    }
}

void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
    const core::KeyCode key_code = fromGLFWKey(key);
    if (key_code == core::KeyCode::None) {
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        core::KeyPressedEvent event(key_code, action == GLFW_REPEAT);
        dispatch(window, event);
    } else if (action == GLFW_RELEASE) {
        core::KeyReleasedEvent event(key_code);
        dispatch(window, event);
    }
}

void charCallback(GLFWwindow* window, unsigned int codepoint)
{
    core::KeyTypedEvent event(codepoint);
    dispatch(window, event);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int)
{
    const core::MouseCode mouse_code = fromGLFWMouseButton(button);
    if (mouse_code == core::MouseCode::None) {
        return;
    }

    if (action == GLFW_PRESS) {
        core::MouseButtonPressedEvent event(mouse_code);
        dispatch(window, event);
    } else if (action == GLFW_RELEASE) {
        core::MouseButtonReleasedEvent event(mouse_code);
        dispatch(window, event);
    }
}

void mousePositionCallback(GLFWwindow* window, double x, double y)
{
    const auto mouse_x = static_cast<float>(x);
    const auto mouse_y = static_cast<float>(y);
    core::Input::recordMouseMoved(mouse_x, mouse_y);

    core::MouseMovedEvent event(mouse_x, mouse_y);
    dispatch(window, event);
}

void mouseScrollCallback(GLFWwindow* window, double x_offset, double y_offset)
{
    const auto scroll_x = static_cast<float>(x_offset);
    const auto scroll_y = static_cast<float>(y_offset);
    core::Input::recordMouseScrolled(scroll_x, scroll_y);

    core::MouseScrolledEvent event(scroll_x, scroll_y);
    dispatch(window, event);
}
} // namespace

GLFWWindow::GLFWWindow(const core::WindowCreateInfo& info)
    : m_info(info)
{
    init();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window.reset(glfwCreateWindow(m_info.width, m_info.height, m_info.title.c_str(), nullptr, nullptr));
    LUNA_ASSERT(m_window, "Failed to create GLFW window.");

    glfwSetWindowUserPointer(m_window.get(), this);
    glfwSetWindowCloseCallback(m_window.get(), windowCloseCallback);
    glfwSetFramebufferSizeCallback(m_window.get(), framebufferSizeCallback);
    glfwSetWindowFocusCallback(m_window.get(), windowFocusCallback);
    glfwSetKeyCallback(m_window.get(), keyCallback);
    glfwSetCharCallback(m_window.get(), charCallback);
    glfwSetMouseButtonCallback(m_window.get(), mouseButtonCallback);
    glfwSetCursorPosCallback(m_window.get(), mousePositionCallback);
    glfwSetScrollCallback(m_window.get(), mouseScrollCallback);

    m_width = m_info.width;
    m_height = m_info.height;

    core::Input::setProvider(this);
}

GLFWWindow::~GLFWWindow()
{
    core::Input::setProvider(nullptr);
    m_window.reset();
    glfwTerminate();
    s_glfw_initialized = false;
}

void GLFWWindow::onUpdate()
{
    core::Input::resetFrameState();
    glfwPollEvents();
}

void GLFWWindow::setEventCallback(const EventCallbackFn& callback)
{
    m_event_callback = callback;
}

void GLFWWindow::init()
{
    if (!s_glfw_initialized) {
        LUNA_ASSERT(glfwInit() == GLFW_TRUE, "Failed to initialize GLFW.");
        s_glfw_initialized = true;
    }
}

bool GLFWWindow::shouldClose()
{
    return glfwWindowShouldClose(m_window.get());
}

rhi::NativeSurfaceHandle GLFWWindow::getNativeHandle() const
{
#if defined(_WIN32)
    return rhi::NativeSurfaceHandle{
        .platform = rhi::NativeSurfaceHandle::Platform::Win32,
        .display = nullptr,
        .window = glfwGetWin32Window(m_window.get()),
    };
#else
    return rhi::NativeSurfaceHandle{};
#endif
}

uint32_t GLFWWindow::getWidth() const
{
    return m_width;
}

uint32_t GLFWWindow::getHeight() const
{
    return m_height;
}

void GLFWWindow::resize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}

bool GLFWWindow::isKeyPressed(core::KeyCode key) const
{
    const int glfw_key = toGLFWKey(key);
    if (glfw_key == GLFW_KEY_UNKNOWN) {
        return false;
    }

    const int state = glfwGetKey(m_window.get(), glfw_key);
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool GLFWWindow::isMouseButtonPressed(core::MouseCode button) const
{
    const int glfw_button = toGLFWMouseButton(button);
    if (glfw_button < 0) {
        return false;
    }

    return glfwGetMouseButton(m_window.get(), glfw_button) == GLFW_PRESS;
}

glm::vec2 GLFWWindow::getMousePosition() const
{
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(m_window.get(), &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

core::CursorMode GLFWWindow::getCursorMode() const
{
    return fromGLFWCursorMode(glfwGetInputMode(m_window.get(), GLFW_CURSOR));
}

void GLFWWindow::setCursorMode(core::CursorMode mode)
{
    glfwSetInputMode(m_window.get(), GLFW_CURSOR, toGLFWCursorMode(mode));
}

void GLFWWindow::setMousePosition(float x, float y)
{
    glfwSetCursorPos(m_window.get(), x, y);
}

void GLFWWindow::setRawMouseMotion(bool enabled)
{
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(m_window.get(), GLFW_RAW_MOUSE_MOTION, enabled ? GLFW_TRUE : GLFW_FALSE);
    }
}

void GLFWWindow::dispatchEvent(core::Event& event)
{
    if (m_event_callback) {
        m_event_callback(event);
    }
}
} // namespace lunalite::platform

namespace lunalite::core {

std::unique_ptr<Window> Window::create(const WindowCreateInfo& info)
{
    return std::make_unique<platform::GLFWWindow>(info);
}

} // namespace lunalite::core
