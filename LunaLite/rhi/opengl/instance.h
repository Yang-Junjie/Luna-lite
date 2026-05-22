#pragma once
#include "../interface/instance.h"

#include <memory>

namespace lunalite::rhi {

class OpenGLInstance final : public Instance {
public:
    ~OpenGLInstance() override;

    BackendType getBackendType() const override
    {
        return BackendType::OpenGL;
    }

    WindowRequirements getWindowRequirements() const override
    {
        WindowRequirements req{.backend = BackendType::OpenGL,
                               .glMajor = 4,
                               .glMinor = 5,
                               .gl_core_profile = true,
                               .gl_debug_context = true};

        return req;
    }

    bool init(WindowHandle window) override;
    void shutdown() override;
    void resize(uint32_t width, uint32_t height) override;
    Device* getDevice() override;

private:
    void* m_native_window = nullptr;
    std::unique_ptr<Device> m_device;
};

} // namespace lunalite::rhi
