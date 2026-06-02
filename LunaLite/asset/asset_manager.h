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
    const std::unordered_map<AssetHandle, AssetMetadata>& getMetadataRegistry() const;

    template <typename T> T* getAsset(AssetHandle handle)
    {
        return AssetDatabase::get().get<T>(handle);
    }

private:
    AssetManager() = default;
    ~AssetManager() = default;

    void registerDefaultImporters();
    bool registerBuiltinAssets();
    Importer* findImporter(const std::filesystem::path& assetPath) const;

    bool discoverAssets(const std::filesystem::path& assetsRoot, std::vector<std::filesystem::path>& assetPaths) const;

    bool importAssets(const std::vector<std::filesystem::path>& assetPaths,
                      std::vector<AssetMetadata>& importedMetadata);

    std::vector<AssetMetadata> importIfNeeded(const std::filesystem::path& assetPath);
    bool registerMetadata(const std::vector<AssetMetadata>& metadataList);
    bool registerMetadata(const AssetMetadata& metadata);
    bool loadAllAssets();
    bool loadAsset(const AssetMetadata& metadata);

    std::vector<std::unique_ptr<Importer>> m_importers;
    std::unordered_map<AssetHandle, AssetMetadata> m_metadata_registry;
    std::unordered_map<std::string, AssetHandle> m_path_handle_map;
};

} // namespace lunalite::asset
