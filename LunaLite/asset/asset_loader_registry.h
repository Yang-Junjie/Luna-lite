#pragma once
#include "asset.h"
#include "asset_metadata.h"

#include <functional>
#include <memory>
#include <unordered_map>

namespace lunalite::asset {

class AssetLoaderRegistry final {
public:
    void registerDefaults();

    bool loadAll(const std::unordered_map<AssetHandle, AssetMetadata>& metadataRegistry) const;
    bool loadAsset(const AssetMetadata& metadata) const;

private:
    using Loader = std::function<std::shared_ptr<Asset>(const AssetMetadata&)>;

    template <typename AssetT>
    void registerLoader(AssetType type, std::shared_ptr<AssetT> (*load)(const AssetMetadata&))
    {
        m_loaders.emplace(type, [load](const AssetMetadata& metadata) -> std::shared_ptr<Asset> {
            return std::static_pointer_cast<Asset>(load(metadata));
        });
    }

    std::unordered_map<AssetType, Loader> m_loaders;
};

} // namespace lunalite::asset
