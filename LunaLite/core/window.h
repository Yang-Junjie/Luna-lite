#pragma once
#include "event.h"
#include "TinyRHI/interface/rhi_types.h"
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

    rhi::WindowRequirements requirements;
};

class Window : public rhi::Surface {
public:
    using EventCallbackFn = std::function<void(Event&)>;
    ~Window() override = default;

    virtual void init() = 0;
    virtual void onUpdate() = 0;
    virtual void setEventCallback(const EventCallbackFn& callback) = 0;
    virtual bool shouldClose() = 0;

    static std::unique_ptr<Window> create(const WindowCreateInfo& info);
};

} // namespace lunalite::core
