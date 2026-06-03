#pragma once
#include "asset_database.h"
#include "asset_importer_registry.h"
#include "asset_loader_registry.h"
#include "asset_metadata_store.h"
#include "asset_scanner.h"

#include <filesystem>
#include <unordered_map>

namespace lunalite::asset {

class AssetManager {
public:
    static AssetManager& get()
    {
        static AssetManager manager;
        return manager;
    }

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    AssetManager(AssetManager&&) = delete;
    AssetManager& operator=(AssetManager&&) = delete;

    bool loadProjectAssets();
    void clear();

    AssetHandle getHandleByRelativePath(const std::filesystem::path& assetPath) const;
    AssetHandle getHandleByFileName(const std::string& assetName) const;
    const AssetMetadata* getMetadata(AssetHandle handle) const;
    const std::unordered_map<AssetHandle, AssetMetadata>& getMetadataRegistry() const;

    template <typename T> T* getAsset(AssetHandle handle)
    {
        return AssetDatabase::get().get<T>(handle);
    }

private:
    AssetManager() = default;
    ~AssetManager() = default;

    bool registerBuiltinAssets();

    AssetScanner m_scanner;
    AssetImporterRegistry m_importers;
    AssetMetadataStore m_metadata;
    AssetLoaderRegistry m_loaders;
};

} // namespace lunalite::asset
