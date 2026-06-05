#include "../metadata/asset_metadata_store.h"
#include "prefab_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

std::vector<AssetMetadata> PrefabAssetImporter::import(const std::filesystem::path& assetPath,
                                                       AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Prefab);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> PrefabAssetImporter::getSupportedExtensions() const
{
    return {".lunaprefab"};
}

} // namespace lunalite::asset
