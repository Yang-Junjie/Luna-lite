#pragma once
#include "../asset_metadata.h"
#include "asset_path_resolver.h"

#include <filesystem>
#include <optional>

namespace lunalite::asset {

class AssetMetadataSerializer final {
public:
    bool write(const AssetMetadata& metadata) const;
    std::optional<AssetMetadata> read(const std::filesystem::path& metadataPath) const;

private:
    AssetPathResolver m_paths;
};

} // namespace lunalite::asset
