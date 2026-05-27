#include "glfw_imgui_platform.h"

#include "../../core/window.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>

#include <cstdint>
#include <memory>

namespace lunalite::platform {

GLFWImGuiPlatform::~GLFWImGuiPlatform()
{
    shutdown();
}

bool GLFWImGuiPlatform::init(core::Window& window)
{
    if (m_window != nullptr) {
        return false;
    }

    auto* glfwWindow = static_cast<GLFWwindow*>(window.getPlatformWindowHandle());
    if (glfwWindow == nullptr) {
        return false;
    }

    if (!ImGui_ImplGlfw_InitForOther(glfwWindow, true)) {
        return false;
    }

    ImGui_ImplGlfw_SetCallbacksChainForAllWindows(true);
    m_window = glfwWindow;
    return true;
}

void GLFWImGuiPlatform::shutdown()
{
    if (m_window == nullptr) {
        return;
    }

    ImGui_ImplGlfw_Shutdown();
    m_window = nullptr;
}

void GLFWImGuiPlatform::newFrame()
{
    ImGui_ImplGlfw_NewFrame();
}

rhi::NativeWindowHandle GLFWImGuiPlatform::nativeWindowHandle(const ImGuiViewport& viewport) const
{
    auto* glfwWindow = static_cast<GLFWwindow*>(viewport.PlatformHandle);
    if (glfwWindow == nullptr) {
        return {};
    }

#if defined(_WIN32)
    return rhi::NativeWindowHandle{
        .platform = rhi::NativeWindowHandle::Platform::Win32,
        .display = nullptr,
        .window = glfwGetWin32Window(glfwWindow),
    };
#elif defined(__APPLE__)
    return rhi::NativeWindowHandle{
        .platform = rhi::NativeWindowHandle::Platform::Cocoa,
        .display = nullptr,
        .window = glfwGetCocoaWindow(glfwWindow),
    };
#elif defined(__linux__)
    return rhi::NativeWindowHandle{
        .platform = rhi::NativeWindowHandle::Platform::X11,
        .display = glfwGetX11Display(),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(glfwWindow))),
    };
#else
    return {};
#endif
}

} // namespace lunalite::platform

namespace lunalite::imgui {

std::unique_ptr<ImGuiPlatform> ImGuiPlatform::create()
{
    return std::make_unique<platform::GLFWImGuiPlatform>();
}

} // namespace lunalite::imgui
