#include "../metadata/asset_metadata_store.h"
#include "sprite_asset_importer.h"

namespace lunalite::asset {

std::vector<AssetMetadata> SpriteAssetImporter::import(const std::filesystem::path& assetPath,
                                                       AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Sprite);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> SpriteAssetImporter::getSupportedExtensions() const
{
    return {".lunasprite"};
}

} // namespace lunalite::asset
