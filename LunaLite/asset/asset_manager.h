#pragma once
#include "asset_database.h"
#include "asset_importer.h"

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

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

    template <typename T> T* getAsset(AssetHandle handle)
    {
        return AssetDatabase::get().get<T>(handle);
    }

private:
    AssetManager() = default;
    ~AssetManager() = default;

    void registerDefaultImporters();
    Importer* findImporter(const std::filesystem::path& assetPath) const;

    bool scanAssetsDirectory(const std::filesystem::path& assetsRoot);
    bool importIfNeeded(const std::filesystem::path& assetPath);
    bool registerMetadata(const AssetMetadata& metadata);
    bool loadAllAssets();
    bool loadAsset(const AssetMetadata& metadata);

    std::vector<std::unique_ptr<Importer>> m_importers;
    std::unordered_map<uint64_t, AssetMetadata> m_metadata_registry;
    std::unordered_map<std::string, AssetHandle> m_path_handle_map;
};

} // namespace lunalite::asset
