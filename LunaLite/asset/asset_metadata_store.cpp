#include "../core/log.h"
#include "asset_metadata_store.h"

#include <optional>

namespace lunalite::asset {

void AssetMetadataStore::clear()
{
    m_metadata_registry.clear();
    m_path_handle_map.clear();
}

bool AssetMetadataStore::registerAll(const std::vector<AssetMetadata>& metadataList)
{
    for (const auto& metadata : metadataList) {
        if (!registerMetadata(metadata)) {
            return false;
        }
    }

    return true;
}

bool AssetMetadataStore::registerMetadata(const AssetMetadata& metadata)
{
    if (!metadata.Handle.isValid()) {
        LUNA_CORE_ERROR("Failed to register asset metadata '{}': invalid handle", metadata.FilePath.string());
        return false;
    }

    if (m_metadata_registry.contains(metadata.Handle)) {
        const auto& existingMetadata = m_metadata_registry.at(metadata.Handle);
        if (existingMetadata.Type == metadata.Type &&
            normalizedProjectPath(existingMetadata.FilePath) == normalizedProjectPath(metadata.FilePath)) {
            return true;
        }

        LUNA_CORE_ERROR("Failed to register asset metadata '{}': duplicate handle {}",
                        metadata.FilePath.string(),
                        metadata.Handle.toString());
        return false;
    }

    m_metadata_registry.emplace(metadata.Handle, metadata);
    m_path_handle_map.emplace(normalizedProjectPath(metadata.FilePath), metadata.Handle);
    return true;
}

AssetHandle AssetMetadataStore::handleByRelativePath(const std::filesystem::path& assetPath) const
{
    if (assetPath.is_absolute()) {
        LUNA_CORE_ERROR("Failed to get asset: path must be relative to the project root: '{}'", assetPath.string());
        return AssetHandle{0};
    }

    const auto relativePath = normalizedProjectPath(assetPath);
    if (const auto metadata = m_path_handle_map.find(relativePath); metadata != m_path_handle_map.end()) {
        return metadata->second;
    }

    return AssetHandle{0};
}

AssetHandle AssetMetadataStore::handleByFileName(const std::string& assetName) const
{
    const std::filesystem::path targetFileName{assetName};
    std::optional<AssetHandle> matchedHandle;
    for (const auto& [path, handle] : m_path_handle_map) {
        if (std::filesystem::path(path).filename() == targetFileName) {
            matchedHandle = handle;
        }
    }

    return matchedHandle.value_or(AssetHandle{0});
}

const AssetMetadata* AssetMetadataStore::find(AssetHandle handle) const
{
    const auto metadata = m_metadata_registry.find(handle);
    if (metadata == m_metadata_registry.end()) {
        return nullptr;
    }

    return &metadata->second;
}

const std::unordered_map<AssetHandle, AssetMetadata>& AssetMetadataStore::all() const
{
    return m_metadata_registry;
}

bool AssetMetadataStore::isMetadataSidecar(const std::filesystem::path& path)
{
    return path.extension() == ".lunameta";
}

std::filesystem::path AssetMetadataStore::sidecarPathFor(const std::filesystem::path& assetPath)
{
    auto metadataPath = assetPath;
    metadataPath += ".lunameta";
    return metadataPath;
}

std::string AssetMetadataStore::normalizedProjectPath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

} // namespace lunalite::asset
