#include "texture_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {
namespace {
YAML::Node defaultTextureConfig()
{
    YAML::Node config;
    config["GenerateMipmaps"] = true;
    config["ColorSpace"] = "SRGB";

    YAML::Node sampler;
    sampler["MinFilter"] = "Linear";
    sampler["MagFilter"] = "Linear";
    sampler["MipFilter"] = "Linear";
    sampler["AddressU"] = "Repeat";
    sampler["AddressV"] = "Repeat";
    sampler["AddressW"] = "Repeat";
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
        metadata.SpecializedConfig = defaultTextureConfig();
    }

    if (!serializeMetadata(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> TextureAssetImporter::getSupportedExtensions() const
{
    return {".png", ".jpg", ".jpeg"};
}

} // namespace lunalite::asset
