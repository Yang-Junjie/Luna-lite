#pragma once
#include "../../animation/sprite_animation.h"
#include "../asset_metadata.h"

#include <memory>

namespace lunalite::asset {

class SpriteAnimationClipLoader {
public:
    static std::shared_ptr<animation::SpriteAnimationClip> load(const AssetMetadata& metadata);
};

} // namespace lunalite::asset
