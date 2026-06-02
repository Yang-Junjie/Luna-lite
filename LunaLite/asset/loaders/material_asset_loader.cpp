#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "material_asset_loader.h"

#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {
glm::vec3 readVec3(const YAML::Node& node, const glm::vec3& fallback)
{
    if (!node || !node.IsSequence() || node.size() != 3) {
        return fallback;
    }

    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>()};
}

glm::vec4 readVec4(const YAML::Node& node, const glm::vec4& fallback)
{
    if (!node || !node.IsSequence() || node.size() != 4) {
        return fallback;
    }

    return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>()};
}

renderer::interface::ShadingModel readShadingModel(const YAML::Node& node)
{
    if (!node) {
        return renderer::interface::ShadingModel::Lit;
    }

    const auto value = node.as<std::string>("Lit");
    if (value == "Unlit") {
        return renderer::interface::ShadingModel::Unlit;
    }

    return renderer::interface::ShadingModel::Lit;
}
} // namespace

std::shared_ptr<renderer::interface::Material> MaterialAssetLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    try {
        const auto root = YAML::LoadFile(path.string());
        const auto materialNode = root["Material"] ? root["Material"] : root;

        auto material = std::make_shared<renderer::interface::Material>();
        material->handle = metadata.Handle;
        auto& parameters = *material->parameters;
        parameters.shading_model = readShadingModel(materialNode["ShadingModel"]);
        parameters.albedo = readVec4(materialNode["Albedo"], parameters.albedo);
        parameters.metallic = materialNode["Metallic"].as<float>(parameters.metallic);
        parameters.roughness = materialNode["Roughness"].as<float>(parameters.roughness);
        parameters.emission = readVec3(materialNode["Emission"], parameters.emission);
        parameters.emission_strength = materialNode["EmissionStrength"].as<float>(parameters.emission_strength);

        if (const auto textures = materialNode["Textures"]) {
            parameters.albedo_texture = AssetHandle{textures["Albedo"].as<uint64_t>(0)};
            parameters.normal_texture = AssetHandle{textures["Normal"].as<uint64_t>(0)};
            parameters.metallic_roughness_texture = AssetHandle{textures["MetallicRoughness"].as<uint64_t>(0)};
        }

        return material;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load material '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load material '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}

} // namespace lunalite::asset
