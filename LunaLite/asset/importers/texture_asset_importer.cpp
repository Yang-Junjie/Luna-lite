#include "texture_asset_importer.h"

#include <algorithm>
#include <filesystem>

namespace lunalite::asset {
namespace {
bool isHdrTexture(const std::filesystem::path& assetPath)
{
    auto extension = assetPath.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".hdr";
}

YAML::Node defaultTextureConfig(const std::filesystem::path& assetPath)
{
    const bool hdr = isHdrTexture(assetPath);

    YAML::Node config;
    config["GenerateMipmaps"] = true;
    config["ColorSpace"] = hdr ? "Linear" : "SRGB";

    YAML::Node sampler;
    sampler["MinFilter"] = "Linear";
    sampler["MagFilter"] = "Linear";
    sampler["MipFilter"] = "Linear";
    sampler["AddressU"] = "Repeat";
    sampler["AddressV"] = hdr ? "ClampToEdge" : "Repeat";
    sampler["AddressW"] = hdr ? "ClampToEdge" : "Repeat";
    config["Sampler"] = sampler;
    return config;
}
} // namespace

std::vector<AssetMetadata> TextureAssetImporter::import(const std::filesystem::path& assetPath)
{
    auto metadata = createMetadata(assetPath, AssetType::Texture);
    const auto metaPath = getMetaFilePath(metadata);

    if (std::filesystem::exists(metaPath)) {
        const auto oldMetadata = deserializeMetadata(metaPath);
        if (oldMetadata.Handle.isValid()) {
            metadata.Handle = oldMetadata.Handle;
        }
        metadata.MemoryOnly = oldMetadata.MemoryOnly;
        metadata.SpecializedConfig = oldMetadata.SpecializedConfig;
    }

    metadata.Type = AssetType::Texture;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = makeProjectRelative(assetPath);
    if (!hasSpecializedConfig(metadata.SpecializedConfig)) {
        metadata.SpecializedConfig = defaultTextureConfig(assetPath);
    }

    if (!serializeMetadata(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> TextureAssetImporter::getSupportedExtensions() const
{
    return {".png", ".jpg", ".jpeg", ".hdr"};
}

} // namespace lunalite::asset
