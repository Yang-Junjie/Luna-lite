#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace lunalite::asset {

class AssetImporterRegistry;

class AssetScanner final {
public:
    std::optional<std::vector<std::filesystem::path>>
        findImportableAssets(const std::filesystem::path& assetsRoot, const AssetImporterRegistry& importers) const;
};

} // namespace lunalite::asset
