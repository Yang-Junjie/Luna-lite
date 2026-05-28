#include "../core/log.h"
#include "../project/project_manager.h"
#include "asset_manager.h"
#include "mesh_asset_importer.h"
#include "mesh_asset_loader.h"

#include <optional>
#include <system_error>

namespace lunalite::asset {
namespace {
bool isMetaFile(const std::filesystem::path& path)
{
    return path.extension() == ".lunameta";
}
} // namespace

bool AssetManager::loadProjectAssets()
{
    registerDefaultImporters();

    clear();

    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();

    auto assetsRoot = *projectRoot;
    const auto& projectInfo = project::ProjectManager::instance().getProjectInfo();
    if (projectInfo && !projectInfo->assets_path.empty()) {
        assetsRoot /= projectInfo->assets_path;
    }

    if (!std::filesystem::exists(assetsRoot)) {
        LUNA_CORE_ERROR("Failed to load project assets: '{}' does not exist", assetsRoot.string());
        return false;
    }

    if (!scanAssetsDirectory(assetsRoot)) {
        return false;
    }

    return loadAllAssets();
}

void AssetManager::clear()
{
    m_metadata_registry.clear();
    m_path_handle_map.clear();
    AssetDatabase::get().clear();
}

AssetHandle AssetManager::getHandleByRelativePath(const std::filesystem::path& assetPath) const
{
    project::ProjectManager::instance().getProjectRootPath();

    if (assetPath.is_absolute()) {
        LUNA_CORE_ERROR("Failed to get asset: path must be relative to the project root: '{}'", assetPath.string());
        return AssetHandle{0};
    }

    const auto relativePath = assetPath.lexically_normal().generic_string();
    if (const auto it = m_path_handle_map.find(relativePath); it != m_path_handle_map.end()) {
        return it->second;
    }

    return AssetHandle{0};
}

AssetHandle AssetManager::getHandleByFileName(const std::string& assetName) const
{
    project::ProjectManager::instance().getProjectRootPath();

    const std::filesystem::path targetFileName{assetName};
    std::optional<std::string> matchedPath;
    for (const auto& [path, handle] : m_path_handle_map) {
        if (std::filesystem::path(path).filename() != targetFileName) {
            continue;
        }

        matchedPath = path;
    }

    if (!matchedPath) {
        return AssetHandle{0};
    }

    return getHandleByRelativePath(*matchedPath);
}

const AssetMetadata* AssetManager::getMetadata(AssetHandle handle) const
{
    const auto it = m_metadata_registry.find(static_cast<uint64_t>(handle));
    if (it == m_metadata_registry.end()) {
        return nullptr;
    }

    return &it->second;
}

void AssetManager::registerDefaultImporters()
{
    if (!m_importers.empty()) {
        return;
    }

    m_importers.push_back(std::make_unique<MeshAssetImporter>());
}

Importer* AssetManager::findImporter(const std::filesystem::path& assetPath) const
{
    for (const auto& importer : m_importers) {
        if (importer->supports(assetPath)) {
            return importer.get();
        }
    }

    return nullptr;
}

bool AssetManager::scanAssetsDirectory(const std::filesystem::path& assetsRoot)
{
    std::error_code error;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsRoot, error)) {
        if (error) {
            LUNA_CORE_ERROR("Failed to scan assets directory '{}': {}", assetsRoot.string(), error.message());
            return false;
        }

        if (!entry.is_regular_file() || isMetaFile(entry.path())) {
            continue;
        }

        if (findImporter(entry.path()) == nullptr) {
            continue;
        }

        if (!importIfNeeded(entry.path())) {
            return false;
        }
    }

    return true;
}

bool AssetManager::importIfNeeded(const std::filesystem::path& assetPath)
{
    auto* importer = findImporter(assetPath);
    if (importer == nullptr) {
        return true;
    }

    auto metaPath = assetPath;
    metaPath += ".lunameta";

    AssetMetadata metadata;
    if (std::filesystem::exists(metaPath)) {
        metadata = Importer::deserializeMetadata(metaPath);
    } else {
        metadata = importer->import(assetPath);
    }

    return registerMetadata(metadata);
}

bool AssetManager::registerMetadata(const AssetMetadata& metadata)
{
    if (!metadata.Handle.isValid()) {
        LUNA_CORE_ERROR("Failed to register asset metadata '{}': invalid handle", metadata.FilePath.string());
        return false;
    }

    const auto key = static_cast<uint64_t>(metadata.Handle);
    if (m_metadata_registry.contains(key)) {
        LUNA_CORE_ERROR("Failed to register asset metadata '{}': duplicate handle {}",
                        metadata.FilePath.string(),
                        metadata.Handle.toString());
        return false;
    }

    m_metadata_registry.emplace(key, metadata);
    m_path_handle_map.emplace(metadata.FilePath.lexically_normal().generic_string(), metadata.Handle);
    return true;
}

bool AssetManager::loadAllAssets()
{
    for (const auto& [_, metadata] : m_metadata_registry) {
        if (!loadAsset(metadata)) {
            return false;
        }
    }

    return true;
}

bool AssetManager::loadAsset(const AssetMetadata& metadata)
{
    if (AssetDatabase::get().contains(metadata.Handle)) {
        return true;
    }

    switch (metadata.Type) {
        case AssetType::Mesh: {
            auto mesh = MeshAssetLoader::loadObj(metadata);
            if (!mesh) {
                return false;
            }
            return AssetDatabase::get().add(mesh).isValid();
        }
        default:
            return false;
    }
}

} // namespace lunalite::asset
