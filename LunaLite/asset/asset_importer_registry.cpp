#include "../core/log.h"
#include "asset_importer_registry.h"
#include "importers/material_asset_importer.h"
#include "importers/mesh/mesh_asset_importer.h"
#include "importers/prefab_asset_importer.h"
#include "importers/script_asset_importer.h"
#include "importers/sprite_asset_importer.h"
#include "importers/texture_asset_importer.h"
#include "metadata/asset_metadata_store.h"

#include <filesystem>

namespace lunalite::asset {

void AssetImporterRegistry::registerDefaults()
{
    if (!m_importers.empty()) {
        return;
    }

    m_importers.push_back(std::make_unique<MeshAssetImporter>());
    m_importers.push_back(std::make_unique<MaterialAssetImporter>());
    m_importers.push_back(std::make_unique<PrefabAssetImporter>());
    m_importers.push_back(std::make_unique<ScriptAssetImporter>());
    m_importers.push_back(std::make_unique<TextureAssetImporter>());
    m_importers.push_back(std::make_unique<SpriteAssetImporter>());
}

Importer* AssetImporterRegistry::findImporter(const std::filesystem::path& assetPath) const
{
    for (const auto& importer : m_importers) {
        if (importer->supports(assetPath)) {
            return importer.get();
        }
    }

    return nullptr;
}

std::optional<std::vector<AssetMetadata>> AssetImporterRegistry::importAsset(const std::filesystem::path& assetPath,
                                                                             AssetMetadataStore& metadataStore) const
{
    auto metadataList = importOrReuseMetadata(assetPath, metadataStore);
    if (metadataList.empty()) {
        LUNA_CORE_ERROR("Failed to import asset '{}': no metadata was produced", assetPath.string());
        return std::nullopt;
    }

    for (const auto& metadata : metadataList) {
        LUNA_CORE_DEBUG("Asset import result: '{}' -> '{}' ({}, handle {})",
                        assetPath.string(),
                        metadata.FilePath.string(),
                        assetTypeToString(metadata.Type),
                        metadata.Handle.toString());
    }

    return metadataList;
}

std::optional<std::vector<AssetMetadata>>
    AssetImporterRegistry::importAll(const std::vector<std::filesystem::path>& assetPaths,
                                     AssetMetadataStore& metadataStore) const
{
    std::vector<AssetMetadata> importedMetadata;
    for (const auto& assetPath : assetPaths) {
        auto metadataList = importAsset(assetPath, metadataStore);
        if (!metadataList) {
            return std::nullopt;
        }

        importedMetadata.insert(importedMetadata.end(), metadataList->begin(), metadataList->end());
    }

    LUNA_CORE_DEBUG(
        "Imported or reused {} metadata entries from {} source files", importedMetadata.size(), assetPaths.size());
    return importedMetadata;
}

std::vector<AssetMetadata> AssetImporterRegistry::importOrReuseMetadata(const std::filesystem::path& assetPath,
                                                                        AssetMetadataStore& metadataStore) const
{
    auto* importer = findImporter(assetPath);
    if (importer == nullptr) {
        LUNA_CORE_ERROR("Failed to import asset '{}': no importer registered for extension '{}'",
                        assetPath.string(),
                        assetPath.extension().string());
        return {};
    }

    const auto metadataPath = AssetMetadataStore::metadataFilePathForAsset(assetPath);
    if (std::filesystem::exists(metadataPath)) {
        const auto metadata = metadataStore.readMetadataFile(metadataPath);
        if (!metadata || !metadata->Handle.isValid()) {
            LUNA_CORE_ERROR("Failed to import asset '{}': metadata file '{}' is missing a valid handle",
                            assetPath.string(),
                            metadataPath.string());
            return {};
        }

        if (importer->shouldRefreshExistingMetadata(*metadata, assetPath)) {
            LUNA_CORE_DEBUG(
                "Importing asset source '{}' (refreshing metadata '{}')", assetPath.string(), metadataPath.string());
            const auto metadataList = importer->import(assetPath, metadataStore);
            LUNA_CORE_DEBUG(
                "Imported asset source '{}' -> {} metadata entries", assetPath.string(), metadataList.size());
            return metadataList;
        }

        LUNA_CORE_DEBUG("Reusing asset metadata '{}' for source '{}' ({}, handle {})",
                        metadataPath.string(),
                        assetPath.string(),
                        assetTypeToString(metadata->Type),
                        metadata->Handle.toString());
        return {*metadata};
    }

    LUNA_CORE_DEBUG("Importing asset source '{}'", assetPath.string());
    const auto metadataList = importer->import(assetPath, metadataStore);
    LUNA_CORE_DEBUG("Imported asset source '{}' -> {} metadata entries", assetPath.string(), metadataList.size());
    return metadataList;
}

} // namespace lunalite::asset
