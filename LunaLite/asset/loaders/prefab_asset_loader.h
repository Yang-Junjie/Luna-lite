#pragma once
#include "../asset.h"
#include "../asset_metadata.h"
#include "../prefab.h"

#include <memory>

namespace lunalite::asset {
class PrefabAssetLoader {
public:
    static std::shared_ptr<Prefab> load(const AssetMetadata& metadata);
};
} // namespace lunalite::asset
