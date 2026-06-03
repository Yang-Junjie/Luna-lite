#pragma once
#include "../asset_metadata.h"

#include <filesystem>

namespace lunalite::asset {

class AssetPathResolver final {
public:
    std::filesystem::path projectRoot() const;
    std::filesystem::path makeProjectRelative(const std::filesystem::path& path) const;
    std::filesystem::path metadataFilePath(const AssetMetadata& metadata) const;
};

} // namespace lunalite::asset
