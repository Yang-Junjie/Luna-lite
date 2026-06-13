#include "../metadata/asset_metadata_store.h"
#include "scene_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

std::vector<AssetMetadata> SceneAssetImporter::import(const std::filesystem::path& assetPath,
                                                      AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Scene);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> SceneAssetImporter::getSupportedExtensions() const
{
    return {".lunascene"};
}

} // namespace lunalite::asset
