#pragma once
#include "asset_factory.h"

#include <memory>
#include <string_view>
#include <vector>

namespace lunalite::asset {

class AssetFactoryManager {
public:
    static AssetFactoryManager& get()
    {
        static AssetFactoryManager manager;
        return manager;
    }

    AssetFactoryManager(const AssetFactoryManager&) = delete;
    AssetFactoryManager& operator=(const AssetFactoryManager&) = delete;
    AssetFactoryManager(AssetFactoryManager&&) = delete;
    AssetFactoryManager& operator=(AssetFactoryManager&&) = delete;

    void registerDefaults();
    std::vector<const AssetFactory*> factoriesFor(const AssetFactoryContext& context);
    AssetFactoryResult create(std::string_view id, const AssetFactoryContext& context);

private:
    AssetFactoryManager() = default;
    ~AssetFactoryManager() = default;

    std::vector<std::unique_ptr<AssetFactory>> m_factories;
};

} // namespace lunalite::asset
