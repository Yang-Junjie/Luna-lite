#pragma once
#include "asset_metadata.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace lunalite::asset {

class AssetMetadataStore final {
public:
    void clear();

    bool registerAll(const std::vector<AssetMetadata>& metadataList);
    bool registerMetadata(const AssetMetadata& metadata);

    AssetHandle handleByRelativePath(const std::filesystem::path& assetPath) const;
    AssetHandle handleByFileName(const std::string& assetName) const;

    const AssetMetadata* find(AssetHandle handle) const;
    const std::unordered_map<AssetHandle, AssetMetadata>& all() const;

    static bool isMetadataSidecar(const std::filesystem::path& path);
    static std::filesystem::path sidecarPathFor(const std::filesystem::path& assetPath);

private:
    static std::string normalizedProjectPath(const std::filesystem::path& path);

    std::unordered_map<AssetHandle, AssetMetadata> m_metadata_registry;
    std::unordered_map<std::string, AssetHandle> m_path_handle_map;
};

} // namespace lunalite::asset
