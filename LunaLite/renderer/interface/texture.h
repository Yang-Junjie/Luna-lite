#pragma once
#include "../../asset/asset.h"

#include <cstdint>

#include <utility>
#include <vector>

namespace lunalite::renderer::interface {
enum class TextureFormat {
    RGBA8_UNorm = 0,
    RGBA32F,
};

enum class TextureColorSpace {
    Linear = 0,
    SRGB,
};

enum class TextureFilter {
    Nearest = 0,
    Linear,
};

enum class TextureMipFilter {
    None = 0,
    Nearest,
    Linear,
};

enum class TextureAddressMode {
    Repeat = 0,
    ClampToEdge,
    MirroredRepeat,
};

struct TextureSamplerDesc {
    TextureFilter min_filter{TextureFilter::Linear};
    TextureFilter mag_filter{TextureFilter::Linear};
    TextureMipFilter mip_filter{TextureMipFilter::Linear};
    TextureAddressMode address_u{TextureAddressMode::Repeat};
    TextureAddressMode address_v{TextureAddressMode::Repeat};
    TextureAddressMode address_w{TextureAddressMode::Repeat};
};

struct TextureImportSettings {
    bool generate_mipmaps{true};
    TextureColorSpace color_space{TextureColorSpace::SRGB};
    TextureSamplerDesc sampler;
};

class Texture : public asset::Asset {
public:
    uint32_t getWidth() const
    {
        return m_width;
    }

    uint32_t getHeight() const
    {
        return m_height;
    }

    TextureFormat getFormat() const
    {
        return m_format;
    }

    const std::vector<uint8_t>& getPixels() const
    {
        return m_pixels;
    }

    const TextureImportSettings& getImportSettings() const
    {
        return m_import_settings;
    }

    void setImportSettings(TextureImportSettings settings)
    {
        m_import_settings = settings;
    }

    void setPixels(uint32_t width, uint32_t height, TextureFormat format, std::vector<uint8_t> pixels)
    {
        m_width = width;
        m_height = height;
        m_format = format;
        m_pixels = std::move(pixels);
    }

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::Texture;
    }

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    TextureFormat m_format{TextureFormat::RGBA8_UNorm};
    TextureImportSettings m_import_settings;
    std::vector<uint8_t> m_pixels;
};
} // namespace lunalite::renderer::interface
