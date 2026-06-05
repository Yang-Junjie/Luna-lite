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

const char* shadingModelName(renderer::interface::ShadingModel model)
{
    switch (model) {
        case renderer::interface::ShadingModel::Unlit:
            return "Unlit";
        case renderer::interface::ShadingModel::Lit:
        default:
            return "Lit";
    }
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
        parameters.normal_scale = materialNode["NormalScale"].as<float>(parameters.normal_scale);
        parameters.occlusion_strength = materialNode["OcclusionStrength"].as<float>(parameters.occlusion_strength);

        if (const auto textures = materialNode["Textures"]) {
            parameters.albedo_texture = AssetHandle{textures["Albedo"].as<uint64_t>(0)};
            parameters.normal_texture = AssetHandle{textures["Normal"].as<uint64_t>(0)};
            parameters.metallic_roughness_texture = AssetHandle{textures["MetallicRoughness"].as<uint64_t>(0)};
            parameters.occlusion_texture = AssetHandle{textures["Occlusion"].as<uint64_t>(0)};
            parameters.emission_texture = AssetHandle{textures["Emission"].as<uint64_t>(0)};
        }

        const auto textureSlotCount = static_cast<unsigned>(parameters.albedo_texture.isValid()) +
                                      static_cast<unsigned>(parameters.normal_texture.isValid()) +
                                      static_cast<unsigned>(parameters.metallic_roughness_texture.isValid()) +
                                      static_cast<unsigned>(parameters.occlusion_texture.isValid()) +
                                      static_cast<unsigned>(parameters.emission_texture.isValid());
        LUNA_CORE_DEBUG("Loaded material '{}' (shading: {}, texture slots: {}, handle {})",
                        path.string(),
                        shadingModelName(parameters.shading_model),
                        textureSlotCount,
                        metadata.Handle.toString());
        return material;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load material '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load material '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}

} // namespace lunalite::asset
