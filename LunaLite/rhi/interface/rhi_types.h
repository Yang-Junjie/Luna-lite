#pragma once

namespace lunalite::rhi {

enum class BackendType {
    OpenGL,
    Vulkan,
    D3D12,
    Metal
};

struct WindowRequirements {
    BackendType backend = BackendType::OpenGL;

    // 只有 OpenGL 会用
    int glMajor = 4;
    int glMinor = 5;
    bool glCoreProfile = true;
    bool glDebugContext = true;
};

struct WindowHandle {
    void* nativeWindow = nullptr;
};

} // namespace lunalite::rhi
