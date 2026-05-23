#pragma once
#include "rhi_types.h"

#include <cstdint>

namespace lunalite::rhi {

enum class TextureFormat {
    RGBA8,
    RGBA16F,
    Depth24Stencil8,
    Depth32F
};

enum class TextureUsage : uint32_t {
    None = 0,
    RenderTarget = 1 << 0,
    DepthStencil = 1 << 1,
    Sampled = 1 << 2,
    CopySrc = 1 << 3,
    CopyDst = 1 << 4
};

constexpr TextureUsage operator|(TextureUsage lhs, TextureUsage rhs)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr TextureUsage operator&(TextureUsage lhs, TextureUsage rhs)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

constexpr TextureUsage& operator|=(TextureUsage& lhs, TextureUsage rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

struct TextureDesc {
    uint32_t width{1};
    uint32_t height{1};
    TextureFormat format{TextureFormat::RGBA8};
    TextureUsage usage{TextureUsage::Sampled};
    uint32_t mip_levels{1};
};

struct TextureViewDesc {
    TextureHandle texture{0};
    TextureFormat format{TextureFormat::RGBA8};
};

} // namespace lunalite::rhi
