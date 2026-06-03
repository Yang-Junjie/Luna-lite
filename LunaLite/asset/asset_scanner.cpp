#include "../core/log.h"
#include "asset_importer_registry.h"
#include "asset_scanner.h"
#include "metadata/asset_metadata_store.h"

#include <system_error>

namespace lunalite::asset {

std::optional<std::vector<std::filesystem::path>>
    AssetScanner::findImportableAssets(const std::filesystem::path& assetsRoot,
                                       const AssetImporterRegistry& importers) const
{
    std::vector<std::filesystem::path> assetPaths;

    std::error_code error;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsRoot, error)) {
        if (error) {
            LUNA_CORE_ERROR("Failed to scan assets directory '{}': {}", assetsRoot.string(), error.message());
            return std::nullopt;
        }

        if (!entry.is_regular_file() || AssetMetadataStore::isMetadataFile(entry.path())) {
            continue;
        }

        if (importers.findImporter(entry.path()) == nullptr) {
            continue;
        }

        assetPaths.push_back(entry.path());
    }

    return assetPaths;
}

} // namespace lunalite::asset
