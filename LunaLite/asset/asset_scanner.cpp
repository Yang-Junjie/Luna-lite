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
    size_t scannedFileCount = 0;
    size_t metadataFileCount = 0;
    size_t unsupportedFileCount = 0;

    std::error_code error;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsRoot, error)) {
        if (error) {
            LUNA_CORE_ERROR("Failed to scan assets directory '{}': {}", assetsRoot.string(), error.message());
            return std::nullopt;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        ++scannedFileCount;
        if (AssetMetadataStore::isMetadataFile(entry.path())) {
            ++metadataFileCount;
            continue;
        }

        if (importers.findImporter(entry.path()) == nullptr) {
            ++unsupportedFileCount;
            continue;
        }

        LUNA_CORE_DEBUG("Found importable asset source '{}'", entry.path().string());
        assetPaths.push_back(entry.path());
    }

    LUNA_CORE_DEBUG("Scanned assets directory '{}' (files: {}, metadata: {}, unsupported: {}, importable: {})",
                    assetsRoot.string(),
                    scannedFileCount,
                    metadataFileCount,
                    unsupportedFileCount,
                    assetPaths.size());
    return assetPaths;
}

} // namespace lunalite::asset
