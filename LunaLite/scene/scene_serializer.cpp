#include "../core/log.h"
#include "components.h"
#include "scene.h"
#include "scene_serializer.h"

#include <cstdint>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace lunalite::scene {
namespace {
YAML::Node writeVec3(const glm::vec3& value)
{
    YAML::Node node;
    node.push_back(value.x);
    node.push_back(value.y);
    node.push_back(value.z);
    return node;
}

glm::vec3 readVec3(const YAML::Node& node, const glm::vec3& fallback = glm::vec3{0.0f})
{
    if (!node || !node.IsSequence() || node.size() != 3) {
        return fallback;
    }

    return {
        node[0].as<float>(),
        node[1].as<float>(),
        node[2].as<float>(),
    };
}

const char* projectionTypeToString(renderer::interface::Camera::ProjectionType type)
{
    switch (type) {
        case renderer::interface::Camera::ProjectionType::Orthographic:
            return "Orthographic";
        case renderer::interface::Camera::ProjectionType::Perspective:
        default:
            return "Perspective";
    }
}

renderer::interface::Camera::ProjectionType stringToProjectionType(const std::string& type)
{
    if (type == "Orthographic") {
        return renderer::interface::Camera::ProjectionType::Orthographic;
    }

    return renderer::interface::Camera::ProjectionType::Perspective;
}
} // namespace

bool SceneSerializer::serialize(const Scene& scene, const std::filesystem::path& scene_path)
{
    YAML::Node root;
    root["Scene"] = scene_path.stem().string();
    root["Settings"]["EnvironmentMap"] = static_cast<uint64_t>(scene.getSettings().environment_map);
    root["Settings"]["EnvironmentIntensity"] = scene.getSettings().environment_intensity;

    YAML::Node entities(YAML::NodeType::Sequence);
    const auto& registry = scene.getRegistry();

    std::vector<entt::entity> sceneEntities;
    const auto addEntity = [&sceneEntities](entt::entity entity) {
        if (std::ranges::find(sceneEntities, entity) == sceneEntities.end()) {
            sceneEntities.push_back(entity);
        }
    };

    for (const auto entity : registry.view<const TagComponent>()) {
        addEntity(entity);
    }
    for (const auto entity : registry.view<const TransformComponent>()) {
        addEntity(entity);
    }
    for (const auto entity : registry.view<const ModelComponent>()) {
        addEntity(entity);
    }
    for (const auto entity : registry.view<const ScriptComponent>()) {
        addEntity(entity);
    }
    for (const auto entity : registry.view<const CameraComponent>()) {
        addEntity(entity);
    }
    for (const auto entity : registry.view<const DirectionalLightComponent>()) {
        addEntity(entity);
    }

    for (const auto entity : sceneEntities) {
        YAML::Node serializedEntity;
        serializedEntity["Entity"] = static_cast<uint32_t>(entity);

        if (registry.all_of<TagComponent>(entity)) {
            const auto& tag = registry.get<TagComponent>(entity);
            YAML::Node node;
            node["Tag"] = tag.tag;
            serializedEntity["TagComponent"] = node;
        }

        if (registry.all_of<TransformComponent>(entity)) {
            const auto& transform = registry.get<TransformComponent>(entity);
            YAML::Node node;
            node["Translation"] = writeVec3(transform.translation);
            node["Rotation"] = writeVec3(glm::eulerAngles(transform.rotation));
            node["Scale"] = writeVec3(transform.scale);
            serializedEntity["TransformComponent"] = node;
        }

        if (registry.all_of<ModelComponent>(entity)) {
            const auto& model = registry.get<ModelComponent>(entity);
            YAML::Node node;
            node["Model"] = static_cast<uint64_t>(model.model);
            serializedEntity["ModelComponent"] = node;
        }

        if (registry.all_of<ScriptComponent>(entity)) {
            const auto& script = registry.get<ScriptComponent>(entity);
            YAML::Node node;
            YAML::Node scripts(YAML::NodeType::Sequence);
            for (const auto& binding : script.scripts) {
                YAML::Node scriptNode;
                scriptNode["Script"] = static_cast<uint64_t>(binding.script);
                scriptNode["Enabled"] = binding.enabled;
                scripts.push_back(scriptNode);
            }
            node["Scripts"] = scripts;
            serializedEntity["ScriptComponent"] = node;
        }

        if (registry.all_of<CameraComponent>(entity)) {
            const auto& camera = registry.get<CameraComponent>(entity);
            YAML::Node node;
            node["Primary"] = camera.primary;
            node["ProjectionType"] = projectionTypeToString(camera.camera.getProjectionType());
            node["Exposure"] = camera.camera.getExposure();
            serializedEntity["CameraComponent"] = node;
        }

        if (registry.all_of<DirectionalLightComponent>(entity)) {
            const auto& light = registry.get<DirectionalLightComponent>(entity);
            YAML::Node node;
            node["Color"] = writeVec3(light.color);
            node["Intensity"] = light.intensity;
            serializedEntity["DirectionalLightComponent"] = node;
        }

        entities.push_back(serializedEntity);
    }

    root["Entities"] = entities;

    if (!scene_path.parent_path().empty()) {
        std::error_code error;
        std::filesystem::create_directories(scene_path.parent_path(), error);
        if (error) {
            LUNA_CORE_ERROR(
                "Failed to create scene directory '{}': {}", scene_path.parent_path().string(), error.message());
            return false;
        }
    }

    std::ofstream out(scene_path);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open scene file for writing: '{}'", scene_path.string());
        return false;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write scene file: '{}'", scene_path.string());
        return false;
    }

    return true;
}

bool SceneSerializer::deserialize(Scene& scene, const std::filesystem::path& scene_path)
{
    if (!std::filesystem::exists(scene_path)) {
        LUNA_CORE_ERROR("Failed to deserialize scene: '{}' does not exist", scene_path.string());
        return false;
    }

    try {
        const YAML::Node root = YAML::LoadFile(scene_path.string());
        const YAML::Node entities = root["Entities"];
        if (!entities || !entities.IsSequence()) {
            LUNA_CORE_ERROR("Failed to deserialize scene '{}': missing Entities", scene_path.string());
            return false;
        }

        scene.clear();
        if (const auto settingsNode = root["Settings"]) {
            scene.getSettings().environment_map = asset::AssetHandle{settingsNode["EnvironmentMap"].as<uint64_t>(0)};
            scene.getSettings().environment_intensity = settingsNode["EnvironmentIntensity"].as<float>(1.0f);
        }

        for (const auto& serializedEntity : entities) {
            const auto entity = scene.createEntity();

            if (const auto tagNode = serializedEntity["TagComponent"]) {
                auto& tag = scene.getComponent<TagComponent>(entity);
                tag.tag = tagNode["Tag"].as<std::string>("");
            }

            if (const auto transformNode = serializedEntity["TransformComponent"]) {
                auto& transform = scene.getComponent<TransformComponent>(entity);
                transform.translation = readVec3(transformNode["Translation"]);
                transform.rotation = glm::quat{readVec3(transformNode["Rotation"])};
                transform.scale = readVec3(transformNode["Scale"], glm::vec3{1.0f});
            }

            if (const auto modelNode = serializedEntity["ModelComponent"]) {
                auto& model = scene.addComponent<ModelComponent>(entity);
                model.model = asset::AssetHandle{modelNode["Model"].as<uint64_t>(0)};
            }

            if (const auto scriptNode = serializedEntity["ScriptComponent"]) {
                auto& script = scene.addComponent<ScriptComponent>(entity);
                if (const auto scriptsNode = scriptNode["Scripts"]; scriptsNode && scriptsNode.IsSequence()) {
                    for (const auto& bindingNode : scriptsNode) {
                        ScriptBinding binding;
                        binding.script = asset::AssetHandle{bindingNode["Script"].as<uint64_t>(0)};
                        binding.enabled = bindingNode["Enabled"].as<bool>(true);
                        script.scripts.push_back(binding);
                    }
                }
            }

            if (const auto cameraNode = serializedEntity["CameraComponent"]) {
                auto& camera = scene.addComponent<CameraComponent>(entity);
                camera.primary = cameraNode["Primary"].as<bool>(true);
                camera.camera.setExposure(cameraNode["Exposure"].as<float>(1.0f));

                const auto projectionType =
                    stringToProjectionType(cameraNode["ProjectionType"].as<std::string>("Perspective"));
                if (projectionType == renderer::interface::Camera::ProjectionType::Orthographic) {
                    camera.camera.setOrthographic(10.0f, 0.1f, 100.0f);
                } else {
                    camera.camera.setPerspective(glm::radians(45.0f), 0.1f, 100.0f);
                }
            }

            if (const auto lightNode = serializedEntity["DirectionalLightComponent"]) {
                auto& light = scene.addComponent<DirectionalLightComponent>(entity);
                light.color = readVec3(lightNode["Color"], glm::vec3{1.0f});
                light.intensity = lightNode["Intensity"].as<float>(1.0f);
            }
        }

        return true;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to deserialize scene '{}': {}", scene_path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to deserialize scene '{}': {}", scene_path.string(), exception.what());
    }

    return false;
}

} // namespace lunalite::scene
