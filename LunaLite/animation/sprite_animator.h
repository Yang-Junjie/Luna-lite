#pragma once
#include "../asset/asset.h"
#include "../scene/components.h"
#include "sprite_animation.h"

namespace lunalite::animation {

struct SpriteAnimatorUpdateResult {
    asset::AssetHandle sprite{0};
};

SpriteAnimatorUpdateResult updateSpriteAnimator(scene::SpriteAnimatorComponent& animator,
                                                const SpriteAnimatorController& controller,
                                                float deltaSeconds);

} // namespace lunalite::animation
