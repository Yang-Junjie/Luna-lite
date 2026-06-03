#pragma once
#include "../asset_metadata.h"
#include "asset_metadata_serializer.h"
#include "asset_path_resolver.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lunalite::asset {

class AssetMetadataStore final {
public:
    void clear();

    bool registerAll(const std::vector<AssetMetadata>& metadataList);
    bool registerMetadata(const AssetMetadata& metadata);

    AssetMetadata createOrLoadMetadata(const std::filesystem::path& assetPath, AssetType type) const;
    AssetMetadata createOrLoadMetadata(const std::filesystem::path& assetPath,
                                       AssetType type,
                                       const YAML::Node& defaultConfig) const;
    AssetMetadata createOrLoadMetadata(const std::filesystem::path& assetPath,
                                       AssetType type,
                                       AssetHandle suggestedHandle,
                                       const YAML::Node& defaultConfig = {}) const;

    bool writeMetadataFile(const AssetMetadata& metadata) const;
    std::optional<AssetMetadata> readMetadataFile(const std::filesystem::path& metadataPath) const;

    AssetHandle handleByRelativePath(const std::filesystem::path& assetPath) const;
    AssetHandle handleByFileName(const std::string& assetName) const;

    const AssetMetadata* find(AssetHandle handle) const;
    const std::unordered_map<AssetHandle, AssetMetadata>& all() const;

    static bool isMetadataFile(const std::filesystem::path& path);
    static std::filesystem::path metadataFilePathForAsset(const std::filesystem::path& assetPath);
    static bool hasSpecializedConfig(const YAML::Node& config);

private:
    AssetMetadata createMetadata(const std::filesystem::path& assetPath, AssetType type) const;
    static std::string normalizedProjectPath(const std::filesystem::path& path);

    AssetPathResolver m_paths;
    AssetMetadataSerializer m_serializer;
    std::unordered_map<AssetHandle, AssetMetadata> m_metadata_registry;
    std::unordered_map<std::string, AssetHandle> m_path_handle_map;
};

} // namespace lunalite::asset
