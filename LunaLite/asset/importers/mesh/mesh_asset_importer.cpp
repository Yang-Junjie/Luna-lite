#include "../../metadata/asset_metadata_store.h"
#include "mesh_asset_importer.h"

#include <cctype>

#include <algorithm>

namespace lunalite::asset {
namespace {
bool isGltfExtension(const std::filesystem::path& assetPath)
{
    auto extension = assetPath.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension == ".gltf" || extension == ".glb";
}
} // namespace

std::vector<AssetMetadata> MeshAssetImporter::import(const std::filesystem::path& assetPath,
                                                     AssetMetadataStore& metadataStore)
{
    auto meshMetadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::Mesh);
    if (!metadataStore.writeMetadataFile(meshMetadata)) {
        return {};
    }

    auto importedMetadata = std::vector<AssetMetadata>{meshMetadata};
    auto derivedMetadata = isGltfExtension(assetPath)
                               ? m_gltfDerivedAssets.generate(
                                     assetPath, meshMetadata, metadataStore, m_materialDefinitions, m_prefabDefinitions)
                               : m_objDerivedAssets.generate(
                                     assetPath, meshMetadata, metadataStore, m_materialDefinitions, m_prefabDefinitions);
    importedMetadata.insert(importedMetadata.end(), derivedMetadata.begin(), derivedMetadata.end());
    return importedMetadata;
}

std::vector<std::string> MeshAssetImporter::getSupportedExtensions() const
{
    return {".obj", ".gltf", ".glb"};
}

bool MeshAssetImporter::shouldRefreshExistingMetadata(const AssetMetadata&, const std::filesystem::path&) const
{
    // Mesh imports emit derived model and material metadata, so the metadata file alone is not the full import result.
    return true;
}

} // namespace lunalite::asset
