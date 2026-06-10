#pragma once

#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/factory/asset_factory_manager.h"

namespace lunalite::scene {
class Scene;
}

namespace lunalite::tooling {

class ToolContext {
public:
    ToolContext(asset::AssetManager* asset_manager = &asset::AssetManager::get(),
                asset::AssetFactoryManager* asset_factory_manager = &asset::AssetFactoryManager::get())
        : m_asset_manager(asset_manager),
          m_asset_factory_manager(asset_factory_manager)
    {}

    ToolContext& setScene(scene::Scene& scene)
    {
        m_scene = &scene;
        return *this;
    }

    scene::Scene* scene() const
    {
        return m_scene;
    }

    asset::AssetManager& assetManager() const
    {
        return m_asset_manager != nullptr ? *m_asset_manager : asset::AssetManager::get();
    }

    asset::AssetFactoryManager& assetFactoryManager() const
    {
        return m_asset_factory_manager != nullptr ? *m_asset_factory_manager : asset::AssetFactoryManager::get();
    }

private:
    scene::Scene* m_scene{nullptr};
    asset::AssetManager* m_asset_manager{nullptr};
    asset::AssetFactoryManager* m_asset_factory_manager{nullptr};
};

} // namespace lunalite::tooling
