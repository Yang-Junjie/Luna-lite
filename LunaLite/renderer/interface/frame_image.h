#pragma once

#include "TinyRHI/interface/rhi_types.h"

#include <cstddef>
#include <cstdint>

#include <variant>

namespace lunalite::renderer::interface {

enum class FrameImageFormat {
    RGBA8_UNorm,
    RGBA16_Float,
    RGBA32_Float
};

enum class FrameImageColorSpace {
    Linear,
    SRGB
};

struct CpuFrameStorage {
    const void* pixels{nullptr};
    size_t row_pitch{0};
};

struct GpuFrameStorage {
    rhi::TextureHandle texture{0};
    rhi::TextureViewHandle view{0};
};

struct FrameImage {
    uint32_t width{0};
    uint32_t height{0};
    FrameImageFormat format{FrameImageFormat::RGBA8_UNorm};
    FrameImageColorSpace color_space{FrameImageColorSpace::Linear};
    std::variant<CpuFrameStorage, GpuFrameStorage> storage{GpuFrameStorage{}};
};

} // namespace lunalite::renderer::interface
