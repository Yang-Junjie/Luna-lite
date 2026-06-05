#include "../../../core/log.h"
#include "material_asset_definition_writer.h"

#include <cstdint>

#include <algorithm>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {
bool hasUsableDiffuseColor(const tinyobj::material_t& material)
{
    return material.diffuse[0] > 0.0f || material.diffuse[1] > 0.0f || material.diffuse[2] > 0.0f;
}

float roughnessOrDefault(const tinyobj::material_t& material)
{
    return material.roughness > 0.0f ? material.roughness : 0.5f;
}

YAML::Node makeVec4Node(float x, float y, float z, float w)
{
    YAML::Node node{YAML::NodeType::Sequence};
    node.push_back(x);
    node.push_back(y);
    node.push_back(z);
    node.push_back(w);
    return node;
}

YAML::Node makeVec3Node(float x, float y, float z)
{
    YAML::Node node{YAML::NodeType::Sequence};
    node.push_back(x);
    node.push_back(y);
    node.push_back(z);
    return node;
}

void writeDefinitionFile(const std::filesystem::path& path, const YAML::Node& root)
{
    std::ofstream out(path);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open material definition for writing: '{}'", path.string());
        return;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write material definition: '{}'", path.string());
    }
}
} // namespace

void MaterialAssetDefinitionWriter::writeDefaultDefinition(const std::filesystem::path& materialPath) const
{
    if (std::filesystem::exists(materialPath)) {
        return;
    }

    YAML::Node material;
    material["ShadingModel"] = "Lit";
    material["Albedo"] = makeVec4Node(0.8f, 0.65f, 0.5f, 1.0f);
    material["Metallic"] = 0.0f;
    material["Roughness"] = 0.5f;
    material["Emission"] = makeVec3Node(0.0f, 0.0f, 0.0f);
    material["EmissionStrength"] = 0.0f;
    material["NormalScale"] = 1.0f;
    material["OcclusionStrength"] = 1.0f;

    YAML::Node root;
    root["Material"] = material;
    writeDefinitionFile(materialPath, root);
}

void MaterialAssetDefinitionWriter::writeObjMaterialDefinition(const std::filesystem::path& materialPath,
                                                               const tinyobj::material_t& material) const
{
    if (std::filesystem::exists(materialPath)) {
        return;
    }

    const auto albedo = hasUsableDiffuseColor(material) ? material.diffuse : nullptr;
    YAML::Node materialNode;
    materialNode["ShadingModel"] = "Lit";
    if (albedo != nullptr) {
        materialNode["Albedo"] = makeVec4Node(albedo[0], albedo[1], albedo[2], 1.0f);
    } else {
        materialNode["Albedo"] = makeVec4Node(0.8f, 0.65f, 0.5f, 1.0f);
    }
    materialNode["Metallic"] = std::clamp(static_cast<float>(material.metallic), 0.0f, 1.0f);
    materialNode["Roughness"] = std::clamp(roughnessOrDefault(material), 0.0f, 1.0f);
    materialNode["Emission"] = makeVec3Node(material.emission[0], material.emission[1], material.emission[2]);
    materialNode["EmissionStrength"] = 1.0f;
    materialNode["NormalScale"] = 1.0f;
    materialNode["OcclusionStrength"] = 1.0f;

    YAML::Node root;
    root["Material"] = materialNode;
    writeDefinitionFile(materialPath, root);
}

void MaterialAssetDefinitionWriter::writeGltfMaterialDefinition(
    const std::filesystem::path& materialPath,
    const fastgltf::Material& material,
    const std::unordered_map<size_t, AssetHandle>& textureHandles,
    bool overwriteExisting) const
{
    if (!overwriteExisting && std::filesystem::exists(materialPath)) {
        return;
    }

    const auto& baseColor = material.pbrData.baseColorFactor;
    const auto& emission = material.emissiveFactor;

    YAML::Node materialNode;
    materialNode["ShadingModel"] = material.unlit ? "Unlit" : "Lit";
    materialNode["Albedo"] = makeVec4Node(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
    materialNode["Metallic"] = std::clamp(static_cast<float>(material.pbrData.metallicFactor), 0.0f, 1.0f);
    materialNode["Roughness"] = std::clamp(static_cast<float>(material.pbrData.roughnessFactor), 0.0f, 1.0f);
    materialNode["Emission"] = makeVec3Node(emission[0], emission[1], emission[2]);
    materialNode["EmissionStrength"] = static_cast<float>(material.emissiveStrength);
    materialNode["NormalScale"] = material.normalTexture ? static_cast<float>(material.normalTexture->scale) : 1.0f;
    materialNode["OcclusionStrength"] =
        material.occlusionTexture ? static_cast<float>(material.occlusionTexture->strength) : 1.0f;

    const auto lookupTextureHandle = [&](const auto& info) -> AssetHandle {
        if (!info) {
            return AssetHandle{0};
        }

        const auto handle = textureHandles.find(info->textureIndex);
        return handle != textureHandles.end() ? handle->second : AssetHandle{0};
    };

    const auto albedoTexture = lookupTextureHandle(material.pbrData.baseColorTexture);
    const auto normalTexture = lookupTextureHandle(material.normalTexture);
    const auto metallicRoughnessTexture = lookupTextureHandle(material.pbrData.metallicRoughnessTexture);
    const auto occlusionTexture = lookupTextureHandle(material.occlusionTexture);
    const auto emissionTexture = lookupTextureHandle(material.emissiveTexture);
    if (albedoTexture.isValid() || normalTexture.isValid() || metallicRoughnessTexture.isValid() ||
        occlusionTexture.isValid() || emissionTexture.isValid()) {
        YAML::Node textures;
        if (albedoTexture.isValid()) {
            textures["Albedo"] = static_cast<uint64_t>(albedoTexture);
        }
        if (normalTexture.isValid()) {
            textures["Normal"] = static_cast<uint64_t>(normalTexture);
        }
        if (metallicRoughnessTexture.isValid()) {
            textures["MetallicRoughness"] = static_cast<uint64_t>(metallicRoughnessTexture);
        }
        if (occlusionTexture.isValid()) {
            textures["Occlusion"] = static_cast<uint64_t>(occlusionTexture);
        }
        if (emissionTexture.isValid()) {
            textures["Emission"] = static_cast<uint64_t>(emissionTexture);
        }
        materialNode["Textures"] = textures;
    }

    YAML::Node root;
    root["Material"] = materialNode;
    writeDefinitionFile(materialPath, root);
}

} // namespace lunalite::asset
