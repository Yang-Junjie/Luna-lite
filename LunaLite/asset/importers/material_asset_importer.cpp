#include "../metadata/asset_metadata_store.h"
#include "material_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

std::vector<AssetMetadata> MaterialAssetImporter::import(const std::filesystem::path& assetPath,
                                                         AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Material);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> MaterialAssetImporter::getSupportedExtensions() const
{
    return {".lunamat"};
}

} // namespace lunalite::asset
