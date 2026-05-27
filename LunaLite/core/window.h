#pragma once
#include "event.h"
#include "TinyRHI/interface/surface.h"

#include <cstdint>

#include <functional>
#include <memory>
#include <string>

namespace lunalite::core {

class WindowCreateInfo {
public:
    uint32_t width{800};
    uint32_t height{600};
    std::string title{"LunaLite"};
};

class Window {
public:
    using EventCallbackFn = std::function<void(Event&)>;
    virtual ~Window() = default;

    virtual void init() = 0;
    virtual void onUpdate() = 0;
    virtual void setEventCallback(const EventCallbackFn& callback) = 0;
    virtual bool shouldClose() = 0;
    virtual rhi::NativeWindowHandle getNativeHandle() const = 0;
    virtual void* getPlatformWindowHandle() const = 0;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;

    static std::unique_ptr<Window> create(const WindowCreateInfo& info);
};

} // namespace lunalite::core
