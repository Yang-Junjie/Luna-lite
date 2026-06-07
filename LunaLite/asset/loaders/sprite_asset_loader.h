#pragma once
#include "../asset_metadata.h"
#include "../sprite.h"

#include <memory>

namespace lunalite::asset {

class SpriteAssetLoader {
public:
    static std::shared_ptr<Sprite> load(const AssetMetadata& metadata);
};

} // namespace lunalite::asset
