#pragma once
#include "asset_importer.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace lunalite::asset {

class AssetMetadataStore;

class AssetImporterRegistry final {
public:
    void registerDefaults();

    Importer* findImporter(const std::filesystem::path& assetPath) const;
    std::optional<std::vector<AssetMetadata>> importAsset(const std::filesystem::path& assetPath,
                                                          AssetMetadataStore& metadataStore) const;
    std::optional<std::vector<AssetMetadata>> importAll(const std::vector<std::filesystem::path>& assetPaths,
                                                        AssetMetadataStore& metadataStore) const;

private:
    std::vector<AssetMetadata> importOrReuseMetadata(const std::filesystem::path& assetPath,
                                                     AssetMetadataStore& metadataStore) const;

    std::vector<std::unique_ptr<Importer>> m_importers;
};

} // namespace lunalite::asset
