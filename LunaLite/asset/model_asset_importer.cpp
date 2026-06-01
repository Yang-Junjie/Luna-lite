#include "model_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

AssetMetadata ModelAssetImporter::import(const std::filesystem::path& assetPath)
{
    auto metadata = createMetadata(assetPath, AssetType::Model);
    const auto metaPath = getMetaFilePath(metadata);

    if (std::filesystem::exists(metaPath)) {
        const auto oldMetadata = deserializeMetadata(metaPath);
        if (oldMetadata.Handle.isValid()) {
            metadata.Handle = oldMetadata.Handle;
        }
        metadata.MemoryOnly = oldMetadata.MemoryOnly;
        metadata.SpecializedConfig = oldMetadata.SpecializedConfig;
    }

    metadata.Type = AssetType::Model;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = makeProjectRelative(assetPath);

    if (!serializeMetadata(metadata)) {
        return {};
    }

    return metadata;
}

std::vector<std::string> ModelAssetImporter::getSupportedExtensions() const
{
    return {".lunamodel"};
}

} // namespace lunalite::asset
