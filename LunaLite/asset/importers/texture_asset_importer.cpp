#include "../metadata/asset_metadata_store.h"
#include "texture_asset_importer.h"

#include <cctype>
#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <string>

namespace lunalite::asset {
namespace {
constexpr uint32_t DefaultIrradianceCubeSize = 32;
constexpr uint32_t DefaultPrefilterCubeSize = 128;

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

    if (hdr) {
        YAML::Node environment;
        environment["Type"] = "EquirectangularHDR";
        environment["IrradianceSize"] = DefaultIrradianceCubeSize;
        environment["PrefilterSize"] = DefaultPrefilterCubeSize;
        config["Environment"] = environment;
    }
    return config;
}
} // namespace

std::vector<AssetMetadata> TextureAssetImporter::import(const std::filesystem::path& assetPath,
                                                        AssetMetadataStore& metadataStore)
{
    auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Texture, defaultTextureConfig(assetPath));

    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> TextureAssetImporter::getSupportedExtensions() const
{
    return {".png", ".jpg", ".jpeg", ".hdr"};
}

bool TextureAssetImporter::shouldRefreshExistingMetadata(const AssetMetadata&,
                                                         const std::filesystem::path& assetPath) const
{
    return false;
}

} // namespace lunalite::asset
