#include "../asset/asset_manager.h"
#include "sprite_animator.h"

#include <cmath>

#include <algorithm>

namespace lunalite::animation {
namespace {
scene::SpriteAnimatorParameterValue defaultValueFor(const AnimatorParameter& parameter)
{
    return scene::SpriteAnimatorParameterValue{
        .bool_value = parameter.default_bool,
        .float_value = parameter.default_float,
        .int_value = parameter.default_int,
        .trigger_value = false,
    };
}

void syncParameters(scene::SpriteAnimatorComponent& animator, const SpriteAnimatorController& controller)
{
    for (const auto& parameter : controller.parameters) {
        if (!animator.parameters.contains(parameter.name)) {
            animator.parameters.emplace(parameter.name, defaultValueFor(parameter));
        }
    }
}

bool compareFloat(float lhs, AnimatorConditionOperator op, float rhs)
{
    switch (op) {
        case AnimatorConditionOperator::NotEquals:
            return std::abs(lhs - rhs) > 0.0001f;
        case AnimatorConditionOperator::Greater:
            return lhs > rhs;
        case AnimatorConditionOperator::GreaterOrEqual:
            return lhs >= rhs;
        case AnimatorConditionOperator::Less:
            return lhs < rhs;
        case AnimatorConditionOperator::LessOrEqual:
            return lhs <= rhs;
        case AnimatorConditionOperator::Equals:
        default:
            return std::abs(lhs - rhs) <= 0.0001f;
    }
}

bool compareInt(int32_t lhs, AnimatorConditionOperator op, int32_t rhs)
{
    switch (op) {
        case AnimatorConditionOperator::NotEquals:
            return lhs != rhs;
        case AnimatorConditionOperator::Greater:
            return lhs > rhs;
        case AnimatorConditionOperator::GreaterOrEqual:
            return lhs >= rhs;
        case AnimatorConditionOperator::Less:
            return lhs < rhs;
        case AnimatorConditionOperator::LessOrEqual:
            return lhs <= rhs;
        case AnimatorConditionOperator::Equals:
        default:
            return lhs == rhs;
    }
}

bool evaluateCondition(const AnimatorCondition& condition,
                       const SpriteAnimatorController& controller,
                       const scene::SpriteAnimatorComponent& animator)
{
    const auto* parameter = controller.findParameter(condition.parameter);
    if (parameter == nullptr) {
        return false;
    }

    const auto valueIt = animator.parameters.find(condition.parameter);
    const auto value = valueIt != animator.parameters.end() ? valueIt->second : defaultValueFor(*parameter);

    switch (parameter->type) {
        case AnimatorParameterType::Bool:
            if (condition.op == AnimatorConditionOperator::NotEquals) {
                return value.bool_value != condition.bool_value;
            }
            return value.bool_value == condition.bool_value;
        case AnimatorParameterType::Float:
            return compareFloat(value.float_value, condition.op, condition.float_value);
        case AnimatorParameterType::Int:
            return compareInt(value.int_value, condition.op, condition.int_value);
        case AnimatorParameterType::Trigger:
            return condition.op == AnimatorConditionOperator::Triggered && value.trigger_value;
    }

    return false;
}

bool transitionConditionsMet(const AnimatorTransition& transition,
                             const SpriteAnimatorController& controller,
                             const scene::SpriteAnimatorComponent& animator)
{
    if (transition.conditions.empty()) {
        return true;
    }

    return std::ranges::all_of(transition.conditions, [&](const AnimatorCondition& condition) {
        return evaluateCondition(condition, controller, animator);
    });
}

bool exitTimeReached(const AnimatorTransition& transition,
                     const SpriteAnimationClip* clip,
                     const scene::SpriteAnimatorComponent& animator)
{
    if (!transition.has_exit_time) {
        return true;
    }
    if (clip == nullptr) {
        return false;
    }

    const float duration = clip->duration();
    if (duration <= 0.0f) {
        return true;
    }
    return animator.state_time >= duration * std::max(transition.exit_time, 0.0f);
}

const AnimatorTransition* findTransition(const SpriteAnimatorController& controller,
                                         const scene::SpriteAnimatorComponent& animator,
                                         const SpriteAnimationClip* currentClip)
{
    for (const auto& transition : controller.transitions) {
        if (!transition.any_state && transition.from != animator.current_state) {
            continue;
        }
        if (transition.to.empty() || transition.to == animator.current_state) {
            continue;
        }
        if (!exitTimeReached(transition, currentClip, animator)) {
            continue;
        }
        if (!transitionConditionsMet(transition, controller, animator)) {
            continue;
        }
        return &transition;
    }
    return nullptr;
}

void consumeTriggers(scene::SpriteAnimatorComponent& animator, const SpriteAnimatorController& controller)
{
    for (const auto& parameter : controller.parameters) {
        if (parameter.type != AnimatorParameterType::Trigger) {
            continue;
        }
        if (auto it = animator.parameters.find(parameter.name); it != animator.parameters.end()) {
            it->second.trigger_value = false;
        }
    }
}
} // namespace

SpriteAnimatorUpdateResult updateSpriteAnimator(scene::SpriteAnimatorComponent& animator,
                                                const SpriteAnimatorController& controller,
                                                float deltaSeconds)
{
    syncParameters(animator, controller);
    if (animator.current_state.empty() || controller.findState(animator.current_state) == nullptr) {
        animator.current_state = controller.defaultStateName();
        animator.state_time = 0.0f;
    }

    const auto* state = controller.findState(animator.current_state);
    if (state == nullptr) {
        return {};
    }

    const auto* clip = asset::AssetManager::get().getAsset<SpriteAnimationClip>(state->clip);
    if (clip == nullptr) {
        return {};
    }

    if (animator.playing) {
        animator.state_time += deltaSeconds * animator.speed;
    }

    if (const auto* transition = findTransition(controller, animator, clip)) {
        animator.current_state = transition->to;
        animator.state_time = 0.0f;
        state = controller.findState(animator.current_state);
        clip = state != nullptr ? asset::AssetManager::get().getAsset<SpriteAnimationClip>(state->clip) : nullptr;
    }

    SpriteAnimatorUpdateResult result;
    if (clip != nullptr) {
        const auto sprite = clip->sample(animator.state_time, state != nullptr ? state->loop : clip->loop);
        if (sprite.isValid()) {
            result.sprite = sprite;
        }
    }

    consumeTriggers(animator, controller);
    return result;
}

} // namespace lunalite::animation
