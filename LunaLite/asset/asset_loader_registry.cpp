#include "asset_database.h"
#include "asset_loader_registry.h"
#include "loaders/material_asset_loader.h"
#include "loaders/mesh_asset_loader.h"
#include "loaders/model_asset_loader.h"
#include "loaders/texture_asset_loader.h"

namespace lunalite::asset {

void AssetLoaderRegistry::registerDefaults()
{
    if (!m_loaders.empty()) {
        return;
    }

    registerLoader(AssetType::Mesh, MeshAssetLoader::load);
    registerLoader(AssetType::Material, MaterialAssetLoader::load);
    registerLoader(AssetType::Model, ModelAssetLoader::load);
    registerLoader(AssetType::Texture, TextureAssetLoader::load);
}

bool AssetLoaderRegistry::loadAll(const std::unordered_map<AssetHandle, AssetMetadata>& metadataRegistry) const
{
    for (const auto& metadataEntry : metadataRegistry) {
        if (!loadAsset(metadataEntry.second)) {
            return false;
        }
    }

    return true;
}

bool AssetLoaderRegistry::loadAsset(const AssetMetadata& metadata) const
{
    if (AssetDatabase::get().contains(metadata.Handle)) {
        return true;
    }

    if (metadata.Type == AssetType::Script) {
        return true;
    }

    const auto loader = m_loaders.find(metadata.Type);
    if (loader == m_loaders.end()) {
        return false;
    }

    auto asset = loader->second(metadata);
    if (!asset) {
        return false;
    }

    return AssetDatabase::get().add(std::move(asset)).isValid();
}

} // namespace lunalite::asset
