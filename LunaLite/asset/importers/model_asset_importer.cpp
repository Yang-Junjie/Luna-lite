#include "../metadata/asset_metadata_store.h"
#include "model_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

std::vector<AssetMetadata> ModelAssetImporter::import(const std::filesystem::path& assetPath,
                                                      AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Model);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> ModelAssetImporter::getSupportedExtensions() const
{
    return {".lunamodel"};
}

} // namespace lunalite::asset
