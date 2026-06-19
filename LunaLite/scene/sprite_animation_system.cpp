#include "../animation/sprite_animator.h"
#include "../asset/asset_manager.h"
#include "components.h"
#include "sprite_animation_system.h"

namespace lunalite::scene {

void updateSpriteAnimators(Scene& scene, core::Timestep dt)
{
    auto view = scene.getRegistry().view<SpriteAnimatorComponent, SpriteRendererComponent>();
    for (const auto entity : view) {
        auto& animator = view.get<SpriteAnimatorComponent>(entity);
        auto& renderer = view.get<SpriteRendererComponent>(entity);
        if (!animator.controller.isValid()) {
            continue;
        }

        const auto* controller =
            asset::AssetManager::get().getAsset<animation::SpriteAnimatorController>(animator.controller);
        if (controller == nullptr) {
            continue;
        }

        const auto result = animation::updateSpriteAnimator(animator, *controller, dt.getSeconds());
        if (result.sprite.isValid()) {
            renderer.sprite = result.sprite;
        }
    }
}

} // namespace lunalite::scene
