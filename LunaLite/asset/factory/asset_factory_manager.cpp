#include "../../core/log.h"
#include "asset_factory_manager.h"
#include "sprite_asset_factory.h"

namespace lunalite::asset {

void AssetFactoryManager::registerDefaults()
{
    if (!m_factories.empty()) {
        return;
    }

    m_factories.push_back(std::make_unique<SpriteAssetFactory>());
}

std::vector<const AssetFactory*> AssetFactoryManager::factoriesFor(const AssetFactoryContext& context)
{
    registerDefaults();

    std::vector<const AssetFactory*> factories;
    for (const auto& factory : m_factories) {
        if (factory->canCreate(context)) {
            factories.push_back(factory.get());
        }
    }
    return factories;
}

AssetFactoryResult AssetFactoryManager::create(std::string_view id, const AssetFactoryContext& context)
{
    registerDefaults();

    for (const auto& factory : m_factories) {
        if (factory->id() != id) {
            continue;
        }
        if (!factory->canCreate(context)) {
            return AssetFactoryResult{.error = "Factory cannot create from the current context"};
        }

        auto result = factory->create(context);
        if (!result.handle.isValid() && !result.error.empty()) {
            LUNA_CORE_ERROR("Asset factory '{}' failed: {}", id, result.error);
        }
        return result;
    }

    return AssetFactoryResult{.error = "Asset factory is not registered"};
}

} // namespace lunalite::asset
