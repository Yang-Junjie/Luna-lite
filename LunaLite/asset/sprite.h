#pragma once
#include "asset.h"

#include <cstdint>

#include <glm/glm.hpp>
#include <string>

namespace lunalite::asset {

struct SpriteSourceRect {
    uint32_t x{0};
    uint32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
};

enum class SpriteColorSpace {
    SRGB,
    Linear
};

inline const char* spriteColorSpaceToString(SpriteColorSpace color_space)
{
    switch (color_space) {
        case SpriteColorSpace::Linear:
            return "Linear";
        case SpriteColorSpace::SRGB:
        default:
            return "SRGB";
    }
}

inline SpriteColorSpace spriteColorSpaceFromString(const std::string& color_space)
{
    if (color_space == "Linear") {
        return SpriteColorSpace::Linear;
    }
    return SpriteColorSpace::SRGB;
}

class Sprite : public Asset {
public:
    AssetHandle texture{0};
    SpriteSourceRect source_rect;
    glm::vec2 pivot{0.5f, 0.5f};
    float pixels_per_unit{100.0f};
    bool flip_y{true};
    SpriteColorSpace color_space{SpriteColorSpace::SRGB};

    AssetType getAssetsType() const override
    {
        return AssetType::Sprite;
    }
};

} // namespace lunalite::asset
