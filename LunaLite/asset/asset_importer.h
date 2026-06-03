#pragma once
#include "asset_metadata.h"

#include <filesystem>
#include <string>
#include <vector>

namespace lunalite::asset {

class AssetMetadataStore;

class Importer {
public:
    virtual ~Importer() = default;
    virtual std::vector<AssetMetadata> import(const std::filesystem::path& assetPath,
                                              AssetMetadataStore& metadataStore) = 0;
    virtual std::vector<std::string> getSupportedExtensions() const = 0;
    virtual bool shouldRefreshExistingMetadata(const AssetMetadata& metadata,
                                               const std::filesystem::path& assetPath) const;

    bool supports(const std::filesystem::path& assetPath) const;
};

} // namespace lunalite::asset
