#pragma once
#include "../../animation/sprite_animation.h"
#include "../asset_metadata.h"

#include <memory>

namespace lunalite::asset {

class SpriteAnimatorControllerLoader {
public:
    static std::shared_ptr<animation::SpriteAnimatorController> load(const AssetMetadata& metadata);
};

} // namespace lunalite::asset
