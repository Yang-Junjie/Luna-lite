#include "../core/log.h"
#include "asset_database.h"
#include "asset_loader_registry.h"
#include "loaders/material_asset_loader.h"
#include "loaders/mesh_asset_loader.h"
#include "loaders/prefab_asset_loader.h"
#include "loaders/sprite_asset_loader.h"
#include "loaders/texture_asset_loader.h"

namespace lunalite::asset {

void AssetLoaderRegistry::registerDefaults()
{
    if (!m_loaders.empty()) {
        return;
    }

    registerLoader(AssetType::Mesh, MeshAssetLoader::load);
    registerLoader(AssetType::Material, MaterialAssetLoader::load);
    registerLoader(AssetType::Prefab, PrefabAssetLoader::load);
    registerLoader(AssetType::Texture, TextureAssetLoader::load);
    registerLoader(AssetType::Sprite, SpriteAssetLoader::load);
}

bool AssetLoaderRegistry::loadAll(const std::unordered_map<AssetHandle, AssetMetadata>& metadataRegistry) const
{
    size_t loadedCount = 0;
    for (const auto& metadataEntry : metadataRegistry) {
        if (!loadAsset(metadataEntry.second)) {
            return false;
        }
        ++loadedCount;
    }

    LUNA_CORE_DEBUG("Processed {} asset metadata entries for loading", loadedCount);
    return true;
}

bool AssetLoaderRegistry::loadAsset(const AssetMetadata& metadata) const
{
    if (!metadata.Handle.isValid()) {
        LUNA_CORE_ERROR("Failed to load asset '{}': invalid metadata handle", metadata.FilePath.string());
        return false;
    }

    if (AssetDatabase::get().contains(metadata.Handle)) {
        LUNA_CORE_DEBUG("Using preloaded asset '{}' ({}, handle {})",
                        metadata.FilePath.string(),
                        assetTypeToString(metadata.Type),
                        metadata.Handle.toString());
        return true;
    }

    if (metadata.Type == AssetType::Script) {
        LUNA_CORE_DEBUG("Registered script asset '{}' for runtime loading (handle {})",
                        metadata.FilePath.string(),
                        metadata.Handle.toString());
        return true;
    }

    const auto loader = m_loaders.find(metadata.Type);
    if (loader == m_loaders.end()) {
        LUNA_CORE_ERROR("Failed to load asset '{}' ({}): no loader registered",
                        metadata.FilePath.string(),
                        assetTypeToString(metadata.Type));
        return false;
    }

    auto asset = loader->second(metadata);
    if (!asset) {
        LUNA_CORE_ERROR("Failed to load asset '{}' ({}): loader returned null",
                        metadata.FilePath.string(),
                        assetTypeToString(metadata.Type));
        return false;
    }

    const auto handle = AssetDatabase::get().add(std::move(asset));
    if (!handle.isValid()) {
        LUNA_CORE_ERROR("Failed to register loaded asset '{}' ({}) in asset database",
                        metadata.FilePath.string(),
                        assetTypeToString(metadata.Type));
        return false;
    }

    LUNA_CORE_DEBUG("Loaded asset '{}' ({}, handle {})",
                    metadata.FilePath.string(),
                    assetTypeToString(metadata.Type),
                    handle.toString());
    return true;
}

} // namespace lunalite::asset
