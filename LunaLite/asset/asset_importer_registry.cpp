#include "asset_importer_registry.h"
#include "asset_metadata_store.h"
#include "importers/material_asset_importer.h"
#include "importers/mesh_asset_importer.h"
#include "importers/model_asset_importer.h"
#include "importers/script_asset_importer.h"
#include "importers/texture_asset_importer.h"

#include <filesystem>

namespace lunalite::asset {

void AssetImporterRegistry::registerDefaults()
{
    if (!m_importers.empty()) {
        return;
    }

    m_importers.push_back(std::make_unique<MeshAssetImporter>());
    m_importers.push_back(std::make_unique<MaterialAssetImporter>());
    m_importers.push_back(std::make_unique<ModelAssetImporter>());
    m_importers.push_back(std::make_unique<ScriptAssetImporter>());
    m_importers.push_back(std::make_unique<TextureAssetImporter>());
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

std::optional<std::vector<AssetMetadata>>
    AssetImporterRegistry::importAll(const std::vector<std::filesystem::path>& assetPaths) const
{
    std::vector<AssetMetadata> importedMetadata;
    for (const auto& assetPath : assetPaths) {
        auto metadataList = importOrReuseMetadata(assetPath);
        if (metadataList.empty()) {
            return std::nullopt;
        }

        importedMetadata.insert(importedMetadata.end(), metadataList.begin(), metadataList.end());
    }

    return importedMetadata;
}

std::vector<AssetMetadata> AssetImporterRegistry::importOrReuseMetadata(const std::filesystem::path& assetPath) const
{
    auto* importer = findImporter(assetPath);
    if (importer == nullptr) {
        return {};
    }

    const auto metadataPath = AssetMetadataStore::sidecarPathFor(assetPath);
    if (std::filesystem::exists(metadataPath)) {
        const auto metadata = Importer::deserializeMetadata(metadataPath);
        if (!metadata.Handle.isValid()) {
            return {};
        }

        if (importer->shouldRefreshExistingMetadata(metadata, assetPath)) {
            return importer->import(assetPath);
        }

        return {metadata};
    }

    return importer->import(assetPath);
}

} // namespace lunalite::asset
