#include "../metadata/asset_metadata_store.h"
#include "script_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

std::vector<AssetMetadata> ScriptAssetImporter::import(const std::filesystem::path& assetPath,
                                                       AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Script);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> ScriptAssetImporter::getSupportedExtensions() const
{
    return {".lua"};
}

} // namespace lunalite::asset
