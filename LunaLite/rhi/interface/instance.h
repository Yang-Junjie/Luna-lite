#pragma once
#include "rhi_types.h"

#include <cstdint>

namespace lunalite::rhi {
class Instance {
public:
    virtual ~Instance() = default;

    virtual BackendType getBackendType() const = 0;
    virtual WindowRequirements getWindowRequirements() const = 0;

    // 窗口创建后，RHI 绑定 native window
    virtual bool initialize(WindowHandle window) = 0;

    virtual void shutdown() = 0;
    virtual void present() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};
} // namespace lunalite::rhi
