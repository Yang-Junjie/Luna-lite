#pragma once
#include "../interface/instance.h"

namespace lunalite::rhi {

class OpenGLInstance final : public Instance {
public:
    OpenGLInstance() = default;
    ~OpenGLInstance() override = default;

    BackendType getBackendType() const override
    {
        return BackendType::OpenGL;
    }

    WindowRequirements getWindowRequirements() const override
    {
        WindowRequirements req{
            .backend = BackendType::OpenGL, .glMajor = 4, .glMinor = 5, .glCoreProfile = true, .glDebugContext = true};

        return req;
    }

    bool initialize(WindowHandle window) override;
    void shutdown() override;
    void present() override;
    void resize(uint32_t width, uint32_t height) override;

private:
    void* m_nativeWindow = nullptr;
};

} // namespace lunalite::rhi
