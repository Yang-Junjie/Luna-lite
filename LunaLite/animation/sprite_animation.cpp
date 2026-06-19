#include "sprite_animation.h"

#include <cmath>

#include <algorithm>

namespace lunalite::animation {
namespace {
float safeFrameDuration(const SpriteAnimationFrame& frame, float fallback)
{
    return frame.duration > 0.0f ? frame.duration : fallback;
}
} // namespace

float SpriteAnimationClip::duration() const
{
    const float fallback = fps > 0.0f ? 1.0f / fps : 1.0f / 12.0f;
    float total = 0.0f;
    for (const auto& frame : frames) {
        total += safeFrameDuration(frame, fallback);
    }
    return total;
}

asset::AssetHandle SpriteAnimationClip::sample(float time) const
{
    return sample(time, loop);
}

asset::AssetHandle SpriteAnimationClip::sample(float time, bool should_loop) const
{
    if (frames.empty()) {
        return {};
    }

    const float fallback = fps > 0.0f ? 1.0f / fps : 1.0f / 12.0f;
    const float clipDuration = duration();
    if (clipDuration <= 0.0f) {
        return frames.front().sprite;
    }

    float sampleTime = std::max(time, 0.0f);
    if (should_loop) {
        sampleTime = std::fmod(sampleTime, clipDuration);
    } else {
        sampleTime = std::min(sampleTime, clipDuration);
    }

    float cursor = 0.0f;
    for (const auto& frame : frames) {
        cursor += safeFrameDuration(frame, fallback);
        if (sampleTime < cursor) {
            return frame.sprite;
        }
    }

    return frames.back().sprite;
}

const AnimatorState* SpriteAnimatorController::findState(const std::string& name) const
{
    const auto it = std::ranges::find_if(states, [&name](const AnimatorState& state) {
        return state.name == name;
    });
    return it != states.end() ? &*it : nullptr;
}

const AnimatorParameter* SpriteAnimatorController::findParameter(const std::string& name) const
{
    const auto it = std::ranges::find_if(parameters, [&name](const AnimatorParameter& parameter) {
        return parameter.name == name;
    });
    return it != parameters.end() ? &*it : nullptr;
}

std::string SpriteAnimatorController::defaultStateName() const
{
    if (!entry_state.empty()) {
        return entry_state;
    }
    return states.empty() ? std::string{} : states.front().name;
}

const char* animatorParameterTypeToString(AnimatorParameterType type)
{
    switch (type) {
        case AnimatorParameterType::Float:
            return "Float";
        case AnimatorParameterType::Int:
            return "Int";
        case AnimatorParameterType::Trigger:
            return "Trigger";
        case AnimatorParameterType::Bool:
        default:
            return "Bool";
    }
}

AnimatorParameterType animatorParameterTypeFromString(const std::string& type)
{
    if (type == "Float") {
        return AnimatorParameterType::Float;
    }
    if (type == "Int") {
        return AnimatorParameterType::Int;
    }
    if (type == "Trigger") {
        return AnimatorParameterType::Trigger;
    }
    return AnimatorParameterType::Bool;
}

const char* animatorConditionOperatorToString(AnimatorConditionOperator op)
{
    switch (op) {
        case AnimatorConditionOperator::NotEquals:
            return "NotEquals";
        case AnimatorConditionOperator::Greater:
            return "Greater";
        case AnimatorConditionOperator::GreaterOrEqual:
            return "GreaterOrEqual";
        case AnimatorConditionOperator::Less:
            return "Less";
        case AnimatorConditionOperator::LessOrEqual:
            return "LessOrEqual";
        case AnimatorConditionOperator::Triggered:
            return "Triggered";
        case AnimatorConditionOperator::Equals:
        default:
            return "Equals";
    }
}

AnimatorConditionOperator animatorConditionOperatorFromString(const std::string& op)
{
    if (op == "NotEquals") {
        return AnimatorConditionOperator::NotEquals;
    }
    if (op == "Greater") {
        return AnimatorConditionOperator::Greater;
    }
    if (op == "GreaterOrEqual") {
        return AnimatorConditionOperator::GreaterOrEqual;
    }
    if (op == "Less") {
        return AnimatorConditionOperator::Less;
    }
    if (op == "LessOrEqual") {
        return AnimatorConditionOperator::LessOrEqual;
    }
    if (op == "Triggered") {
        return AnimatorConditionOperator::Triggered;
    }
    return AnimatorConditionOperator::Equals;
}

} // namespace lunalite::animation
