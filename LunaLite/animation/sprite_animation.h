#pragma once
#include "../asset/asset.h"

#include <cstdint>

#include <string>
#include <vector>

namespace lunalite::animation {

struct SpriteAnimationFrame {
    asset::AssetHandle sprite{0};
    float duration{1.0f / 12.0f};
};

class SpriteAnimationClip : public asset::Asset {
public:
    std::vector<SpriteAnimationFrame> frames;
    float fps{12.0f};
    bool loop{true};

    float duration() const;
    asset::AssetHandle sample(float time) const;
    asset::AssetHandle sample(float time, bool should_loop) const;

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::SpriteAnimationClip;
    }
};

enum class AnimatorParameterType {
    Bool,
    Float,
    Int,
    Trigger
};

enum class AnimatorConditionOperator {
    Equals,
    NotEquals,
    Greater,
    GreaterOrEqual,
    Less,
    LessOrEqual,
    Triggered
};

struct AnimatorParameter {
    std::string name;
    AnimatorParameterType type{AnimatorParameterType::Bool};
    bool default_bool{false};
    float default_float{0.0f};
    int32_t default_int{0};
};

struct AnimatorCondition {
    std::string parameter;
    AnimatorConditionOperator op{AnimatorConditionOperator::Equals};
    bool bool_value{false};
    float float_value{0.0f};
    int32_t int_value{0};
};

struct AnimatorState {
    std::string name;
    asset::AssetHandle clip{0};
    bool loop{true};
};

struct AnimatorTransition {
    std::string from;
    std::string to;
    bool any_state{false};
    bool has_exit_time{false};
    float exit_time{1.0f};
    std::vector<AnimatorCondition> conditions;
};

class SpriteAnimatorController : public asset::Asset {
public:
    std::vector<AnimatorParameter> parameters;
    std::vector<AnimatorState> states;
    std::vector<AnimatorTransition> transitions;
    std::string entry_state;

    const AnimatorState* findState(const std::string& name) const;
    const AnimatorParameter* findParameter(const std::string& name) const;
    std::string defaultStateName() const;

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::SpriteAnimatorController;
    }
};

const char* animatorParameterTypeToString(AnimatorParameterType type);
AnimatorParameterType animatorParameterTypeFromString(const std::string& type);
const char* animatorConditionOperatorToString(AnimatorConditionOperator op);
AnimatorConditionOperator animatorConditionOperatorFromString(const std::string& op);

} // namespace lunalite::animation
