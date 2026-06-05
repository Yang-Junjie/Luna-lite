#include "../core/log.h"
#include "../project/project_manager.h"
#include "asset_manager.h"
#include "builtin/builtin_assets.h"
#include "builtin/material_factory.h"
#include "builtin/mesh_factory.h"
#include "builtin/model_factory.h"

#include <string>
#include <utility>

namespace lunalite::asset {
namespace {
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
    m_importers.registerDefaults();
    m_loaders.registerDefaults();

    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    const auto& projectInfo = project::ProjectManager::instance().getProjectInfo();
    if (!projectRoot || !projectInfo) {
        LUNA_CORE_ERROR("Failed to load project assets: no project is loaded");
        return false;
    }

    if (projectInfo->assets_path.is_absolute()) {
        LUNA_CORE_ERROR("Failed to load project assets: assets path must be relative to the project root: '{}'",
                        projectInfo->assets_path.string());
        return false;
    }

    const auto assetsRoot = (*projectRoot / projectInfo->assets_path).lexically_normal();
    if (!std::filesystem::exists(assetsRoot) || !std::filesystem::is_directory(assetsRoot)) {
        LUNA_CORE_ERROR("Failed to load project assets: '{}' is not a directory", assetsRoot.string());
        return false;
    }

    LUNA_CORE_INFO("Loading project assets from '{}'", assetsRoot.string());
    clear();

    if (!registerBuiltinAssets()) {
        LUNA_CORE_ERROR("Failed to load project assets: built-in asset registration failed");
        return false;
    }

    const auto assetPaths = m_scanner.findImportableAssets(assetsRoot, m_importers);
    if (!assetPaths) {
        LUNA_CORE_ERROR("Failed to load project assets: asset scan failed");
        return false;
    }

    LUNA_CORE_DEBUG("Asset scan found {} importable source files", assetPaths->size());
    const auto importedMetadata = m_importers.importAll(*assetPaths, m_metadata);
    if (!importedMetadata) {
        LUNA_CORE_ERROR("Failed to load project assets: asset import failed");
        return false;
    }

    if (!m_metadata.registerAll(*importedMetadata)) {
        LUNA_CORE_ERROR("Failed to load project assets: metadata registration failed");
        return false;
    }

    if (!m_loaders.loadAll(m_metadata.all())) {
        LUNA_CORE_ERROR("Failed to load project assets: asset loading failed");
        return false;
    }

    LUNA_CORE_INFO(
        "Loaded project assets ({} source files, {} metadata entries)", assetPaths->size(), m_metadata.all().size());
    return true;
}

void AssetManager::clear()
{
    if (!m_metadata.all().empty()) {
        LUNA_CORE_DEBUG("Clearing {} asset metadata entries", m_metadata.all().size());
    }
    m_metadata.clear();
    AssetDatabase::get().clear();
}

AssetHandle AssetManager::getHandleByRelativePath(const std::filesystem::path& assetPath) const
{
    return m_metadata.handleByRelativePath(assetPath);
}

AssetHandle AssetManager::getHandleByFileName(const std::string& assetName) const
{
    return m_metadata.handleByFileName(assetName);
}

const AssetMetadata* AssetManager::getMetadata(AssetHandle handle) const
{
    return m_metadata.find(handle);
}

const std::unordered_map<AssetHandle, AssetMetadata>& AssetManager::getMetadataRegistry() const
{
    return m_metadata.all();
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

    LUNA_ASSERT(defaultMaterialMetadata.Handle.isValid() && errorMaterialMetadata.Handle.isValid() &&
                    cubeMeshMetadata.Handle.isValid() && planeMeshMetadata.Handle.isValid() &&
                    cubeModelMetadata.Handle.isValid() && planeModelMetadata.Handle.isValid(),
                "Built-in asset handles must be valid.");

    if (!m_metadata.registerMetadata(defaultMaterialMetadata) || !m_metadata.registerMetadata(errorMaterialMetadata) ||
        !m_metadata.registerMetadata(cubeMeshMetadata) || !m_metadata.registerMetadata(planeMeshMetadata) ||
        !m_metadata.registerMetadata(cubeModelMetadata) || !m_metadata.registerMetadata(planeModelMetadata)) {
        return false;
    }

    return AssetDatabase::get().add(MaterialFactory::createDefault()).isValid() &&
           AssetDatabase::get().add(MaterialFactory::createError()).isValid() &&
           AssetDatabase::get().add(MeshFactory::createCube()).isValid() &&
           AssetDatabase::get().add(MeshFactory::createPlane()).isValid() &&
           AssetDatabase::get().add(ModelFactory::createCube()).isValid() &&
           AssetDatabase::get().add(ModelFactory::createPlane()).isValid();
}

} // namespace lunalite::asset
