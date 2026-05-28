#include "mesh_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

AssetMetadata MeshAssetImporter::import(const std::filesystem::path& assetPath)
{
    auto metadata = createMetadata(assetPath, AssetType::Mesh);
    const auto metaPath = getMetaFilePath(metadata);

    if (std::filesystem::exists(metaPath)) {
        const auto oldMetadata = deserializeMetadata(metaPath);
        if (oldMetadata.Handle.isValid()) {
            metadata.Handle = oldMetadata.Handle;
        }
        metadata.MemoryOnly = oldMetadata.MemoryOnly;
        metadata.SpecializedConfig = oldMetadata.SpecializedConfig;
    }

    metadata.Type = AssetType::Mesh;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = makeProjectRelative(assetPath);

    if (!serializeMetadata(metadata)) {
        return {};
    }

    return metadata;
}

std::vector<std::string> MeshAssetImporter::getSupportedExtensions() const
{
    return {".obj"};
}

} // namespace lunalite::asset
