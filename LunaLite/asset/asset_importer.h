#pragma once
#include "../project/project_manager.h"
#include "asset_metadata.h"

#include <filesystem>
#include <string>
#include <vector>

namespace lunalite::asset {
class Importer {
public:
    virtual ~Importer() = default;
    virtual std::vector<AssetMetadata> import(const std::filesystem::path& assetPath) = 0;
    virtual std::vector<std::string> getSupportedExtensions() const = 0;

    bool supports(const std::filesystem::path& assetPath) const;

    static std::filesystem::path getProjectRoot();
    static std::filesystem::path makeProjectRelative(const std::filesystem::path& path);
    static std::filesystem::path getMetaFilePath(const AssetMetadata& metadata);
    static bool serializeMetadata(const AssetMetadata& metadata);
    static AssetMetadata deserializeMetadata(const std::filesystem::path& metaPath);

protected:
    static AssetMetadata createMetadata(const std::filesystem::path& assetPath, AssetType type);
};

} // namespace lunalite::asset
