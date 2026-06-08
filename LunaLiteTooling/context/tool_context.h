#pragma once

#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/factory/asset_factory_manager.h"

namespace lunalite::tooling {

class ToolContext {
public:
    ToolContext(asset::AssetManager* asset_manager = &asset::AssetManager::get(),
                asset::AssetFactoryManager* asset_factory_manager = &asset::AssetFactoryManager::get())
        : m_asset_manager(asset_manager),
          m_asset_factory_manager(asset_factory_manager)
    {}

    asset::AssetManager& assetManager() const
    {
        return m_asset_manager != nullptr ? *m_asset_manager : asset::AssetManager::get();
    }

    asset::AssetFactoryManager& assetFactoryManager() const
    {
        return m_asset_factory_manager != nullptr ? *m_asset_factory_manager : asset::AssetFactoryManager::get();
    }

private:
    asset::AssetManager* m_asset_manager{nullptr};
    asset::AssetFactoryManager* m_asset_factory_manager{nullptr};
};

} // namespace lunalite::tooling
