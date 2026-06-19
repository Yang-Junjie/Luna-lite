#include "../LunaLite/animation/sprite_animation.h"
#include "../LunaLite/asset/asset_database.h"
#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/sprite_animation_system.h"

#include <cmath>

#include <iostream>
#include <memory>

namespace {
bool nearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= 0.001f;
}
} // namespace

int main()
{
    using namespace lunalite;

    auto clip = std::make_shared<animation::SpriteAnimationClip>();
    clip->handle = asset::AssetHandle{10};
    clip->fps = 2.0f;
    clip->loop = true;
    clip->frames = {
        {.sprite = asset::AssetHandle{101}, .duration = 0.25f},
        {.sprite = asset::AssetHandle{102}, .duration = 0.25f},
        {.sprite = asset::AssetHandle{103}, .duration = 0.5f},
    };
    if (!nearlyEqual(clip->duration(), 1.0f) || clip->sample(0.0f) != asset::AssetHandle{101} ||
        clip->sample(0.25f) != asset::AssetHandle{102} || clip->sample(0.5f) != asset::AssetHandle{103} ||
        clip->sample(1.25f) != asset::AssetHandle{102} || clip->sample(2.0f, false) != asset::AssetHandle{103}) {
        std::cerr << "SpriteAnimationClip did not sample frames as expected.\n";
        return 1;
    }

    auto idleClip = std::make_shared<animation::SpriteAnimationClip>();
    idleClip->handle = asset::AssetHandle{20};
    idleClip->frames = {{.sprite = asset::AssetHandle{201}, .duration = 1.0f}};
    auto runClip = std::make_shared<animation::SpriteAnimationClip>();
    runClip->handle = asset::AssetHandle{30};
    runClip->frames = {{.sprite = asset::AssetHandle{301}, .duration = 1.0f}};

    auto controller = std::make_shared<animation::SpriteAnimatorController>();
    controller->handle = asset::AssetHandle{40};
    controller->entry_state = "Idle";
    controller->parameters.push_back({
        .name = "Running",
        .type = animation::AnimatorParameterType::Bool,
        .default_bool = false,
    });
    controller->states.push_back({
        .name = "Idle",
        .clip = idleClip->handle,
        .loop = true,
    });
    controller->states.push_back({
        .name = "Run",
        .clip = runClip->handle,
        .loop = true,
    });
    controller->transitions.push_back({
        .from = "Idle",
        .to = "Run",
        .conditions = {{
            .parameter = "Running",
            .op = animation::AnimatorConditionOperator::Equals,
            .bool_value = true,
        }},
    });

    asset::AssetDatabase::get().clear();
    asset::AssetDatabase::get().add(clip);
    asset::AssetDatabase::get().add(idleClip);
    asset::AssetDatabase::get().add(runClip);
    asset::AssetDatabase::get().add(controller);

    scene::Scene scene;
    const auto entity = scene.createEntity();
    auto& renderer = scene.addComponent<scene::SpriteRendererComponent>(entity);
    auto& animator = scene.addComponent<scene::SpriteAnimatorComponent>(entity);
    animator.controller = controller->handle;

    scene::updateSpriteAnimators(scene, core::Timestep{0.0f});
    if (animator.current_state != "Idle" || renderer.sprite != asset::AssetHandle{201} ||
        !animator.parameters.contains("Running") || animator.parameters["Running"].bool_value) {
        std::cerr << "Sprite animator did not enter its default state.\n";
        return 1;
    }

    animator.parameters["Running"].bool_value = true;
    scene::updateSpriteAnimators(scene, core::Timestep{0.016f});
    if (animator.current_state != "Run" || renderer.sprite != asset::AssetHandle{301} ||
        !nearlyEqual(animator.state_time, 0.0f)) {
        std::cerr << "Sprite animator did not select the expected transition.\n";
        return 1;
    }

    auto instantController = std::make_shared<animation::SpriteAnimatorController>();
    instantController->handle = asset::AssetHandle{50};
    instantController->entry_state = "A";
    instantController->states.push_back({.name = "A", .clip = idleClip->handle, .loop = true});
    instantController->states.push_back({.name = "B", .clip = runClip->handle, .loop = true});
    instantController->transitions.push_back({.from = "A", .to = "B"});
    asset::AssetDatabase::get().add(instantController);

    animator.controller = instantController->handle;
    animator.current_state.clear();
    animator.state_time = 0.0f;
    animator.parameters.clear();
    scene::updateSpriteAnimators(scene, core::Timestep{0.0f});
    if (animator.current_state != "B" || renderer.sprite != asset::AssetHandle{301}) {
        std::cerr << "Sprite animator did not execute an unconditional immediate transition.\n";
        return 1;
    }

    std::cout << "Sprite animation clips sample frames and animator controllers transition between states.\n";
    return 0;
}
