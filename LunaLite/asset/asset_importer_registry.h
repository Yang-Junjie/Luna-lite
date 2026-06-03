#pragma once
#include "asset_importer.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace lunalite::asset {

class AssetImporterRegistry final {
public:
    void registerDefaults();

    Importer* findImporter(const std::filesystem::path& assetPath) const;
    std::optional<std::vector<AssetMetadata>> importAll(const std::vector<std::filesystem::path>& assetPaths) const;

private:
    std::vector<AssetMetadata> importOrReuseMetadata(const std::filesystem::path& assetPath) const;

    std::vector<std::unique_ptr<Importer>> m_importers;
};

} // namespace lunalite::asset
