#include "../../core/log.h"
#include "asset_metadata_store.h"

#include <filesystem>
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

AssetMetadata AssetMetadataStore::createOrLoadMetadata(const std::filesystem::path& assetPath, AssetType type) const
{
    return createOrLoadMetadata(assetPath, type, AssetHandle{0});
}

AssetMetadata AssetMetadataStore::createOrLoadMetadata(const std::filesystem::path& assetPath,
                                                       AssetType type,
                                                       const YAML::Node& defaultConfig) const
{
    return createOrLoadMetadata(assetPath, type, AssetHandle{0}, defaultConfig);
}

AssetMetadata AssetMetadataStore::createOrLoadMetadata(const std::filesystem::path& assetPath,
                                                       AssetType type,
                                                       AssetHandle suggestedHandle,
                                                       const YAML::Node& defaultConfig) const
{
    auto metadata = createMetadata(assetPath, type);
    const auto metadataPath = m_paths.metadataFilePath(metadata);

    if (std::filesystem::exists(metadataPath)) {
        if (const auto existingMetadata = m_serializer.read(metadataPath)) {
            if (existingMetadata->Handle.isValid()) {
                metadata.Handle = existingMetadata->Handle;
            }
            metadata.MemoryOnly = existingMetadata->MemoryOnly;
            metadata.SpecializedConfig = existingMetadata->SpecializedConfig;
        }
    } else if (suggestedHandle.isValid()) {
        metadata.Handle = suggestedHandle;
    }

    if (!hasSpecializedConfig(metadata.SpecializedConfig) && hasSpecializedConfig(defaultConfig)) {
        metadata.SpecializedConfig = defaultConfig;
    }

    return metadata;
}

bool AssetMetadataStore::writeMetadataFile(const AssetMetadata& metadata) const
{
    return m_serializer.write(metadata);
}

std::optional<AssetMetadata> AssetMetadataStore::readMetadataFile(const std::filesystem::path& metadataPath) const
{
    return m_serializer.read(metadataPath);
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

bool AssetMetadataStore::isMetadataFile(const std::filesystem::path& path)
{
    return path.extension() == ".lunameta";
}

std::filesystem::path AssetMetadataStore::metadataFilePathForAsset(const std::filesystem::path& assetPath)
{
    auto metadataPath = assetPath;
    metadataPath += ".lunameta";
    return metadataPath;
}

bool AssetMetadataStore::hasSpecializedConfig(const YAML::Node& config)
{
    return config.IsDefined() && !config.IsNull();
}

AssetMetadata AssetMetadataStore::createMetadata(const std::filesystem::path& assetPath, AssetType type) const
{
    AssetMetadata metadata;
    metadata.Handle = AssetHandle{};
    metadata.Type = type;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = m_paths.makeProjectRelative(assetPath);
    metadata.MemoryOnly = false;
    return metadata;
}

std::string AssetMetadataStore::normalizedProjectPath(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

} // namespace lunalite::asset
