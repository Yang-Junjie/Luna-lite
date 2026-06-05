#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "texture_asset_loader.h"

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <stb_image.h>
#include <string>
#include <vector>

namespace lunalite::asset {
namespace {
renderer::interface::TextureFilter readTextureFilter(const YAML::Node& node,
                                                     renderer::interface::TextureFilter fallback)
{
    const auto value = node.as<std::string>("");
    if (value == "Nearest") {
        return renderer::interface::TextureFilter::Nearest;
    }
    if (value == "Linear") {
        return renderer::interface::TextureFilter::Linear;
    }
    return fallback;
}

renderer::interface::TextureMipFilter readTextureMipFilter(const YAML::Node& node,
                                                           renderer::interface::TextureMipFilter fallback)
{
    const auto value = node.as<std::string>("");
    if (value == "None") {
        return renderer::interface::TextureMipFilter::None;
    }
    if (value == "Nearest") {
        return renderer::interface::TextureMipFilter::Nearest;
    }
    if (value == "Linear") {
        return renderer::interface::TextureMipFilter::Linear;
    }
    return fallback;
}

renderer::interface::TextureAddressMode readTextureAddressMode(const YAML::Node& node,
                                                               renderer::interface::TextureAddressMode fallback)
{
    const auto value = node.as<std::string>("");
    if (value == "Repeat") {
        return renderer::interface::TextureAddressMode::Repeat;
    }
    if (value == "ClampToEdge") {
        return renderer::interface::TextureAddressMode::ClampToEdge;
    }
    if (value == "MirroredRepeat") {
        return renderer::interface::TextureAddressMode::MirroredRepeat;
    }
    return fallback;
}

renderer::interface::TextureColorSpace readTextureColorSpace(const YAML::Node& node,
                                                             renderer::interface::TextureColorSpace fallback)
{
    const auto value = node.as<std::string>("");
    if (value == "Linear") {
        return renderer::interface::TextureColorSpace::Linear;
    }
    if (value == "SRGB") {
        return renderer::interface::TextureColorSpace::SRGB;
    }
    return fallback;
}

renderer::interface::TextureImportSettings readTextureImportSettings(const YAML::Node& node)
{
    renderer::interface::TextureImportSettings settings;
    if (!node) {
        return settings;
    }

    settings.generate_mipmaps = node["GenerateMipmaps"].as<bool>(settings.generate_mipmaps);
    if (node["ColorSpace"]) {
        settings.color_space = readTextureColorSpace(node["ColorSpace"], settings.color_space);
    }

    const auto sampler = node["Sampler"] ? node["Sampler"] : node;
    settings.sampler.min_filter = readTextureFilter(sampler["MinFilter"], settings.sampler.min_filter);
    settings.sampler.mag_filter = readTextureFilter(sampler["MagFilter"], settings.sampler.mag_filter);
    settings.sampler.mip_filter = readTextureMipFilter(sampler["MipFilter"], settings.sampler.mip_filter);
    settings.sampler.address_u = readTextureAddressMode(sampler["AddressU"], settings.sampler.address_u);
    settings.sampler.address_v = readTextureAddressMode(sampler["AddressV"], settings.sampler.address_v);
    settings.sampler.address_w = readTextureAddressMode(sampler["AddressW"], settings.sampler.address_w);
    return settings;
}

bool isHdrTexture(const std::filesystem::path& path)
{
    auto extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".hdr" || stbi_is_hdr(path.string().c_str()) != 0;
}
} // namespace

std::shared_ptr<renderer::interface::Texture> TextureAssetLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    int width = 0;
    int height = 0;
    int channels = 0;
    auto settings = readTextureImportSettings(metadata.SpecializedConfig);
    auto texture = std::make_shared<renderer::interface::Texture>();
    texture->handle = metadata.Handle;

    if (isHdrTexture(path)) {
        float* rawPixels = stbi_loadf(path.string().c_str(), &width, &height, &channels, 4);
        if (rawPixels == nullptr) {
            LUNA_CORE_ERROR("Failed to load HDR texture '{}': {}", path.string(), stbi_failure_reason());
            return nullptr;
        }

        if (width <= 0 || height <= 0) {
            stbi_image_free(rawPixels);
            LUNA_CORE_ERROR("Failed to load HDR texture '{}': invalid dimensions {}x{}", path.string(), width, height);
            return nullptr;
        }

        const auto byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4 * sizeof(float);
        std::vector<uint8_t> pixels(byteCount);
        std::memcpy(pixels.data(), rawPixels, byteCount);
        stbi_image_free(rawPixels);

        settings.color_space = renderer::interface::TextureColorSpace::Linear;
        texture->setImportSettings(settings);
        texture->setPixels(static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height),
                           renderer::interface::TextureFormat::RGBA32F,
                           std::move(pixels));
        LUNA_CORE_DEBUG("Loaded HDR texture '{}' ({}x{}, source channels: {}, handle {})",
                        path.string(),
                        width,
                        height,
                        channels,
                        metadata.Handle.toString());
        return texture;
    }

    stbi_uc* rawPixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (rawPixels == nullptr) {
        LUNA_CORE_ERROR("Failed to load texture '{}': {}", path.string(), stbi_failure_reason());
        return nullptr;
    }

    if (width <= 0 || height <= 0) {
        stbi_image_free(rawPixels);
        LUNA_CORE_ERROR("Failed to load texture '{}': invalid dimensions {}x{}", path.string(), width, height);
        return nullptr;
    }

    const auto pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    std::vector<uint8_t> pixels(rawPixels, rawPixels + pixelCount);
    stbi_image_free(rawPixels);

    texture->setImportSettings(settings);
    texture->setPixels(static_cast<uint32_t>(width),
                       static_cast<uint32_t>(height),
                       renderer::interface::TextureFormat::RGBA8_UNorm,
                       std::move(pixels));
    LUNA_CORE_DEBUG("Loaded texture '{}' ({}x{}, source channels: {}, handle {})",
                    path.string(),
                    width,
                    height,
                    channels,
                    metadata.Handle.toString());
    return texture;
}

} // namespace lunalite::asset
