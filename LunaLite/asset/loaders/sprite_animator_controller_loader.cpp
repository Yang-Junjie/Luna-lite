#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "sprite_animator_controller_loader.h"

#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {
animation::AnimatorParameter readParameter(const YAML::Node& node)
{
    animation::AnimatorParameter parameter;
    parameter.name = node["Name"].as<std::string>("");
    parameter.type = animation::animatorParameterTypeFromString(node["Type"].as<std::string>("Bool"));
    parameter.default_bool = node["DefaultBool"].as<bool>(false);
    parameter.default_float = node["DefaultFloat"].as<float>(0.0f);
    parameter.default_int = node["DefaultInt"].as<int32_t>(0);
    return parameter;
}

animation::AnimatorState readState(const YAML::Node& node)
{
    return animation::AnimatorState{
        .name = node["Name"].as<std::string>(""),
        .clip = AssetHandle{node["Clip"].as<uint64_t>(0)},
        .loop = node["Loop"].as<bool>(true),
    };
}

animation::AnimatorCondition readCondition(const YAML::Node& node)
{
    animation::AnimatorCondition condition;
    condition.parameter = node["Parameter"].as<std::string>("");
    condition.op = animation::animatorConditionOperatorFromString(node["Operator"].as<std::string>("Equals"));
    condition.bool_value = node["BoolValue"].as<bool>(false);
    condition.float_value = node["FloatValue"].as<float>(0.0f);
    condition.int_value = node["IntValue"].as<int32_t>(0);
    return condition;
}

animation::AnimatorTransition readTransition(const YAML::Node& node)
{
    animation::AnimatorTransition transition;
    transition.from = node["From"].as<std::string>("");
    transition.to = node["To"].as<std::string>("");
    transition.any_state = node["AnyState"].as<bool>(transition.from == "Any");
    transition.has_exit_time = node["HasExitTime"].as<bool>(false);
    transition.exit_time = node["ExitTime"].as<float>(1.0f);
    if (const auto conditionsNode = node["Conditions"]; conditionsNode && conditionsNode.IsSequence()) {
        transition.conditions.reserve(conditionsNode.size());
        for (const auto& conditionNode : conditionsNode) {
            auto condition = readCondition(conditionNode);
            if (!condition.parameter.empty()) {
                transition.conditions.push_back(std::move(condition));
            }
        }
    }
    return transition;
}
} // namespace

std::shared_ptr<animation::SpriteAnimatorController> SpriteAnimatorControllerLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    try {
        const auto root = YAML::LoadFile(path.string());
        const auto controllerNode = root["SpriteAnimatorController"] ? root["SpriteAnimatorController"] : root;

        auto controller = std::make_shared<animation::SpriteAnimatorController>();
        controller->handle = metadata.Handle;
        controller->entry_state = controllerNode["EntryState"].as<std::string>("");

        if (const auto parametersNode = controllerNode["Parameters"]; parametersNode && parametersNode.IsSequence()) {
            controller->parameters.reserve(parametersNode.size());
            for (const auto& parameterNode : parametersNode) {
                auto parameter = readParameter(parameterNode);
                if (!parameter.name.empty()) {
                    controller->parameters.push_back(std::move(parameter));
                }
            }
        }

        if (const auto statesNode = controllerNode["States"]; statesNode && statesNode.IsSequence()) {
            controller->states.reserve(statesNode.size());
            for (const auto& stateNode : statesNode) {
                auto state = readState(stateNode);
                if (!state.name.empty()) {
                    controller->states.push_back(std::move(state));
                }
            }
        }

        if (const auto transitionsNode = controllerNode["Transitions"];
            transitionsNode && transitionsNode.IsSequence()) {
            controller->transitions.reserve(transitionsNode.size());
            for (const auto& transitionNode : transitionsNode) {
                auto transition = readTransition(transitionNode);
                if (!transition.to.empty() && (transition.any_state || !transition.from.empty())) {
                    controller->transitions.push_back(std::move(transition));
                }
            }
        }

        if (!controller->states.empty() && controller->findState(controller->defaultStateName()) == nullptr) {
            LUNA_CORE_ERROR("Failed to load sprite animator controller '{}': invalid entry state '{}'",
                            path.string(),
                            controller->entry_state);
            return nullptr;
        }

        LUNA_CORE_DEBUG("Loaded sprite animator controller '{}' (states: {}, transitions: {}, handle {})",
                        path.string(),
                        controller->states.size(),
                        controller->transitions.size(),
                        metadata.Handle.toString());
        return controller;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load sprite animator controller '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load sprite animator controller '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}

} // namespace lunalite::asset
