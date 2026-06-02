#include "../core/log.h"
#include "../project/project_manager.h"
#include "asset_manager.h"
#include "builtin/builtin_assets.h"
#include "builtin/material_factory.h"
#include "builtin/mesh_factory.h"
#include "builtin/model_factory.h"
#include "importers/material_asset_importer.h"
#include "importers/mesh_asset_importer.h"
#include "importers/model_asset_importer.h"
#include "importers/script_asset_importer.h"
#include "loaders/material_asset_loader.h"
#include "loaders/mesh_asset_loader.h"
#include "loaders/model_asset_loader.h"

#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace lunalite::asset {
namespace {
bool isMetaFile(const std::filesystem::path& path)
{
    return path.extension() == ".lunameta";
}

AssetMetadata
    createBuiltinMetadata(AssetHandle handle, AssetType type, std::string name, std::filesystem::path filePath)
{
    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = type;
    metadata.Name = std::move(name);
    metadata.FilePath = std::move(filePath);
    metadata.MemoryOnly = true;
    return metadata;
}
} // namespace

bool AssetManager::loadProjectAssets()
{
    registerDefaultImporters();

    clear();

    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();

    const auto assetsRoot = *projectRoot;

    if (!std::filesystem::exists(assetsRoot)) {
        LUNA_CORE_ERROR("Failed to load project assets: '{}' does not exist", assetsRoot.string());
        return false;
    }

    if (!registerBuiltinAssets()) {
        return false;
    }

    if (!scanAssetsDirectory(assetsRoot)) {
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
    const auto it = m_metadata_registry.find(handle);
    if (it == m_metadata_registry.end()) {
        return nullptr;
    }

    return &it->second;
}

const std::unordered_map<AssetHandle, AssetMetadata>& AssetManager::getMetadataRegistry() const
{
    return m_metadata_registry;
}

void AssetManager::registerDefaultImporters()
{
    if (!m_importers.empty()) {
        return;
    }

    m_importers.push_back(std::make_unique<MeshAssetImporter>());
    m_importers.push_back(std::make_unique<MaterialAssetImporter>());
    m_importers.push_back(std::make_unique<ModelAssetImporter>());
    m_importers.push_back(std::make_unique<ScriptAssetImporter>());
}

bool AssetManager::registerBuiltinAssets()
{
    const auto defaultMaterialMetadata = createBuiltinMetadata(
        builtin::defaultMaterialHandle(), AssetType::Material, "DefaultMaterial", "Builtin/Materials/Default.lunamat");
    const auto errorMaterialMetadata = createBuiltinMetadata(
        builtin::errorMaterialHandle(), AssetType::Material, "ErrorMaterial", "Builtin/Materials/Error.lunamat");
    const auto cubeMeshMetadata =
        createBuiltinMetadata(builtin::cubeMeshHandle(), AssetType::Mesh, "CubeMesh", "Builtin/Meshes/Cube.mesh");
    const auto planeMeshMetadata =
        createBuiltinMetadata(builtin::planeMeshHandle(), AssetType::Mesh, "PlaneMesh", "Builtin/Meshes/Plane.mesh");
    const auto cubeModelMetadata =
        createBuiltinMetadata(builtin::cubeModelHandle(), AssetType::Model, "CubeModel", "Builtin/Models/Cube.model");
    const auto planeModelMetadata = createBuiltinMetadata(
        builtin::planeModelHandle(), AssetType::Model, "PlaneModel", "Builtin/Models/Plane.model");

    if (!registerMetadata(defaultMaterialMetadata) || !registerMetadata(errorMaterialMetadata) ||
        !registerMetadata(cubeMeshMetadata) || !registerMetadata(planeMeshMetadata) ||
        !registerMetadata(cubeModelMetadata) || !registerMetadata(planeModelMetadata)) {
        return false;
    }

    return AssetDatabase::get().add(MaterialFactory::createDefault()).isValid() &&
           AssetDatabase::get().add(MaterialFactory::createError()).isValid() &&
           AssetDatabase::get().add(MeshFactory::createCube()).isValid() &&
           AssetDatabase::get().add(MeshFactory::createPlane()).isValid() &&
           AssetDatabase::get().add(ModelFactory::createCube()).isValid() &&
           AssetDatabase::get().add(ModelFactory::createPlane()).isValid();
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
        if (metadata.Type == AssetType::Mesh) {
            metadata = importer->import(assetPath);
        }
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

    if (m_metadata_registry.contains(metadata.Handle)) {
        const auto& existing = m_metadata_registry.at(metadata.Handle);
        if (existing.Type == metadata.Type && existing.FilePath.lexically_normal().generic_string() ==
                                                  metadata.FilePath.lexically_normal().generic_string()) {
            return true;
        }

        LUNA_CORE_ERROR("Failed to register asset metadata '{}': duplicate handle {}",
                        metadata.FilePath.string(),
                        metadata.Handle.toString());
        return false;
    }

    m_metadata_registry.emplace(metadata.Handle, metadata);
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
        case AssetType::Material: {
            auto material = MaterialAssetLoader::load(metadata);
            if (!material) {
                return false;
            }
            return AssetDatabase::get().add(material).isValid();
        }
        case AssetType::Model: {
            auto model = ModelAssetLoader::load(metadata);
            if (!model) {
                return false;
            }
            return AssetDatabase::get().add(model).isValid();
        }
        case AssetType::Script:
            return true;
        default:
            return false;
    }
}

} // namespace lunalite::asset
