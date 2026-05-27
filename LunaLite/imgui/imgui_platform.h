#pragma once

#include "TinyRHI/interface/surface.h"

#include <memory>

struct ImGuiViewport;

namespace lunalite::core {
class Window;
}

namespace lunalite::imgui {

class ImGuiPlatform {
public:
    virtual ~ImGuiPlatform() = default;

    virtual bool init(core::Window& window) = 0;
    virtual void shutdown() = 0;
    virtual void newFrame() = 0;
    virtual rhi::NativeWindowHandle nativeWindowHandle(const ImGuiViewport& viewport) const = 0;

    static std::unique_ptr<ImGuiPlatform> create();
};

} // namespace lunalite::imgui
