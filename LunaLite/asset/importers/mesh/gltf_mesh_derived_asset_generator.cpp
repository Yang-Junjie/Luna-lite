#include "../../../core/log.h"
#include "../../metadata/asset_metadata_store.h"
#include "gltf_mesh_derived_asset_generator.h"
#include "material_asset_definition_writer.h"
#include "model_asset_definition_writer.h"

#include <cctype>

#include <algorithm>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace lunalite::asset {
namespace {
std::string sanitizeAssetName(std::string value)
{
    if (value.empty()) {
        return "Default";
    }

    for (auto& character : value) {
        const auto symbol = static_cast<unsigned char>(character);
        if (!std::isalnum(symbol) && character != '-' && character != '_') {
            character = '_';
        }
    }

    return value;
}

std::string indexedAssetName(const std::string& prefix, size_t index, const std::string& name)
{
    std::ostringstream stream;
    stream << prefix << "_Material_" << std::setw(3) << std::setfill('0') << index << "_" << name;
    return stream.str();
}

bool isRenderablePrimitive(const fastgltf::Primitive& primitive)
{
    return primitive.type == fastgltf::PrimitiveType::Triangles &&
           primitive.findAttribute("POSITION") != primitive.attributes.cend();
}

glm::mat4 toGlmMatrix(const fastgltf::math::fmat4x4& matrix)
{
    glm::mat4 result{1.0f};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            result[column][row] = matrix[static_cast<size_t>(column)][static_cast<size_t>(row)];
        }
    }
    return result;
}

fastgltf::Expected<fastgltf::Asset> loadGltfAsset(const std::filesystem::path& path)
{
    static constexpr auto extensions =
        fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_unlit;
    static constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                    fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!gltfFile) {
        return gltfFile.error();
    }

    fastgltf::Parser parser(extensions);
    return parser.loadGltf(gltfFile.get(), path.parent_path(), options);
}

std::string textureFilter(fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return "Nearest";
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
            return "Linear";
    }

    return "Linear";
}

std::string textureMipFilter(fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::Linear:
            return "None";
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return "Nearest";
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
            return "Linear";
    }

    return "Linear";
}

std::string textureAddressMode(fastgltf::Wrap wrap)
{
    switch (wrap) {
        case fastgltf::Wrap::ClampToEdge:
            return "ClampToEdge";
        case fastgltf::Wrap::MirroredRepeat:
            return "MirroredRepeat";
        case fastgltf::Wrap::Repeat:
            return "Repeat";
    }

    return "Repeat";
}

YAML::Node gltfTextureConfig(const fastgltf::Asset& asset, const fastgltf::Texture& texture, const char* colorSpace)
{
    YAML::Node config;
    config["GenerateMipmaps"] = true;
    config["ColorSpace"] = colorSpace;

    YAML::Node sampler;
    if (texture.samplerIndex && *texture.samplerIndex < asset.samplers.size()) {
        const auto& gltfSampler = asset.samplers[*texture.samplerIndex];
        sampler["MinFilter"] = gltfSampler.minFilter ? textureFilter(*gltfSampler.minFilter) : "Linear";
        sampler["MagFilter"] = gltfSampler.magFilter ? textureFilter(*gltfSampler.magFilter) : "Linear";
        sampler["MipFilter"] = gltfSampler.minFilter ? textureMipFilter(*gltfSampler.minFilter) : "Linear";
        sampler["AddressU"] = textureAddressMode(gltfSampler.wrapS);
        sampler["AddressV"] = textureAddressMode(gltfSampler.wrapT);
        sampler["AddressW"] = "Repeat";
    } else {
        sampler["MinFilter"] = "Linear";
        sampler["MagFilter"] = "Linear";
        sampler["MipFilter"] = "Linear";
        sampler["AddressU"] = "Repeat";
        sampler["AddressV"] = "Repeat";
        sampler["AddressW"] = "Repeat";
    }

    config["Sampler"] = sampler;
    return config;
}

std::filesystem::path
    imagePathForTexture(const std::filesystem::path& sourceMeshPath, const fastgltf::Asset& asset, size_t textureIndex)
{
    if (textureIndex >= asset.textures.size()) {
        return {};
    }

    const auto& texture = asset.textures[textureIndex];
    if (!texture.imageIndex || *texture.imageIndex >= asset.images.size()) {
        return {};
    }

    const auto& image = asset.images[*texture.imageIndex];
    const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data);
    if (uri == nullptr || !uri->uri.isLocalPath() || uri->uri.isDataUri()) {
        return {};
    }

    return (sourceMeshPath.parent_path() / uri->uri.fspath()).lexically_normal();
}

AssetMetadata createDerivedMetadataFile(AssetMetadataStore& metadataStore,
                                        const std::filesystem::path& assetPath,
                                        AssetType type,
                                        AssetHandle suggestedHandle,
                                        const YAML::Node& defaultConfig = {})
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, type, suggestedHandle, defaultConfig);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return metadata;
}

std::vector<std::pair<uint32_t, uint32_t>> calculateMeshSubmeshRanges(const fastgltf::Asset& asset)
{
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    ranges.reserve(asset.meshes.size());

    uint32_t nextSubmesh = 0;
    for (const auto& mesh : asset.meshes) {
        const auto submeshStart = nextSubmesh;
        uint32_t submeshCount = 0;
        for (const auto& primitive : mesh.primitives) {
            if (!isRenderablePrimitive(primitive)) {
                continue;
            }
            ++submeshCount;
            ++nextSubmesh;
        }
        ranges.emplace_back(submeshStart, submeshCount);
    }

    return ranges;
}

std::vector<ModelMeshDefinition> buildSceneModelDefinitions(fastgltf::Asset& asset,
                                                            AssetHandle meshHandle,
                                                            const std::vector<AssetHandle>& materialHandles)
{
    std::vector<ModelMeshDefinition> definitions;
    const auto meshRanges = calculateMeshSubmeshRanges(asset);

    const auto appendMeshNode = [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
        if (!node.meshIndex || *node.meshIndex >= meshRanges.size()) {
            return;
        }

        const auto [submeshStart, submeshCount] = meshRanges[*node.meshIndex];
        if (submeshCount == 0) {
            return;
        }

        ModelMeshDefinition definition;
        definition.mesh = meshHandle;
        definition.materials = materialHandles;
        definition.transform = toGlmMatrix(matrix);
        definition.submesh_start = submeshStart;
        definition.submesh_count = submeshCount;
        definitions.push_back(std::move(definition));
    };

    const auto traverseNode =
        [&](const auto& self, size_t nodeIndex, const fastgltf::math::fmat4x4& parentTransform) -> void {
        if (nodeIndex >= asset.nodes.size()) {
            return;
        }

        const auto& node = asset.nodes[nodeIndex];
        const auto transform = fastgltf::getTransformMatrix(node, parentTransform);
        appendMeshNode(node, transform);

        for (const auto child : node.children) {
            self(self, child, transform);
        }
    };

    if (!asset.scenes.empty()) {
        const auto sceneIndex = asset.defaultScene.value_or(0);
        if (sceneIndex < asset.scenes.size()) {
            fastgltf::iterateSceneNodes(asset,
                                        sceneIndex,
                                        fastgltf::math::fmat4x4{},
                                        [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
                                            appendMeshNode(node, matrix);
                                        });
        }
    } else {
        std::vector<bool> referencedByParent(asset.nodes.size(), false);
        for (const auto& node : asset.nodes) {
            for (const auto child : node.children) {
                if (child < referencedByParent.size()) {
                    referencedByParent[child] = true;
                }
            }
        }

        for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex) {
            if (!referencedByParent[nodeIndex]) {
                traverseNode(traverseNode, nodeIndex, fastgltf::math::fmat4x4{});
            }
        }
    }

    if (definitions.empty()) {
        ModelMeshDefinition definition;
        definition.mesh = meshHandle;
        definition.materials = materialHandles;
        definitions.push_back(std::move(definition));
    }

    return definitions;
}
} // namespace

std::vector<AssetMetadata>
    GltfMeshDerivedAssetGenerator::generate(const std::filesystem::path& sourceMeshPath,
                                            const AssetMetadata& meshMetadata,
                                            AssetMetadataStore& metadataStore,
                                            const MaterialAssetDefinitionWriter& materialDefinitions,
                                            const ModelAssetDefinitionWriter& modelDefinitions) const
{
    auto loadedAsset = loadGltfAsset(sourceMeshPath);
    if (!loadedAsset) {
        LUNA_CORE_WARN("Failed to inspect glTF materials for '{}': {}",
                       sourceMeshPath.string(),
                       fastgltf::getErrorMessage(loadedAsset.error()));
        return {};
    }

    auto asset = std::move(loadedAsset.get());
    std::vector<AssetMetadata> derivedMetadata;
    std::unordered_map<size_t, AssetHandle> textureHandles;
    std::unordered_set<AssetHandle> emittedTextureMetadata;

    const auto createTextureHandle = [&](size_t textureIndex, const char* colorSpace) -> AssetHandle {
        if (const auto handle = textureHandles.find(textureIndex); handle != textureHandles.end()) {
            return handle->second;
        }
        if (textureIndex >= asset.textures.size()) {
            return AssetHandle{0};
        }

        const auto imagePath = imagePathForTexture(sourceMeshPath, asset, textureIndex);
        if (imagePath.empty() || !std::filesystem::exists(imagePath)) {
            return AssetHandle{0};
        }

        const auto textureMetadata = createDerivedMetadataFile(
            metadataStore,
            imagePath,
            AssetType::Texture,
            AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 10'000 + static_cast<uint64_t>(textureIndex)},
            gltfTextureConfig(asset, asset.textures[textureIndex], colorSpace));
        if (!textureMetadata.Handle.isValid()) {
            return AssetHandle{0};
        }

        textureHandles.emplace(textureIndex, textureMetadata.Handle);
        if (!emittedTextureMetadata.contains(textureMetadata.Handle)) {
            derivedMetadata.push_back(textureMetadata);
            emittedTextureMetadata.insert(textureMetadata.Handle);
        }

        return textureMetadata.Handle;
    };

    std::vector<AssetHandle> materialHandles;
    if (!asset.materials.empty()) {
        materialHandles.resize(asset.materials.size());
        for (size_t materialIndex = 0; materialIndex < asset.materials.size(); ++materialIndex) {
            const auto& material = asset.materials[materialIndex];
            if (material.pbrData.baseColorTexture) {
                createTextureHandle(material.pbrData.baseColorTexture->textureIndex, "SRGB");
            }
            if (material.normalTexture) {
                createTextureHandle(material.normalTexture->textureIndex, "Linear");
            }
            if (material.pbrData.metallicRoughnessTexture) {
                createTextureHandle(material.pbrData.metallicRoughnessTexture->textureIndex, "Linear");
            }
            if (material.occlusionTexture) {
                createTextureHandle(material.occlusionTexture->textureIndex, "Linear");
            }
            if (material.emissiveTexture) {
                createTextureHandle(material.emissiveTexture->textureIndex, "SRGB");
            }

            const auto materialName = sanitizeAssetName(
                material.name.empty() ? "Material" + std::to_string(materialIndex) : std::string{material.name});
            const auto materialPath =
                sourceMeshPath.parent_path() /
                (indexedAssetName(sourceMeshPath.stem().string(), materialIndex, materialName) + ".lunamat");
            materialDefinitions.writeGltfMaterialDefinition(materialPath, material, textureHandles, true);

            const auto materialMetadata = createDerivedMetadataFile(
                metadataStore,
                materialPath,
                AssetType::Material,
                AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2 + static_cast<uint64_t>(materialIndex)});
            if (!materialMetadata.Handle.isValid()) {
                continue;
            }

            materialHandles[materialIndex] = materialMetadata.Handle;
            derivedMetadata.push_back(materialMetadata);
        }
    }

    const auto hasValidMaterialHandle = std::ranges::any_of(materialHandles, [](AssetHandle handle) {
        return handle.isValid();
    });

    if (!hasValidMaterialHandle) {
        const auto materialPath = sourceMeshPath.parent_path() / (sourceMeshPath.stem().string() + "_Default.lunamat");
        materialDefinitions.writeDefaultDefinition(materialPath);

        const auto materialMetadata =
            createDerivedMetadataFile(metadataStore,
                                      materialPath,
                                      AssetType::Material,
                                      AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2});
        if (materialMetadata.Handle.isValid()) {
            materialHandles.clear();
            materialHandles.push_back(materialMetadata.Handle);
            derivedMetadata.push_back(materialMetadata);
        }
    }

    const auto modelPath = sourceMeshPath.parent_path() / (sourceMeshPath.stem().string() + ".lunamodel");
    modelDefinitions.writeDefinition(
        modelPath, buildSceneModelDefinitions(asset, meshMetadata.Handle, materialHandles), true);

    const auto modelMetadata = createDerivedMetadataFile(
        metadataStore, modelPath, AssetType::Model, AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 1});
    if (modelMetadata.Handle.isValid()) {
        derivedMetadata.push_back(modelMetadata);
    }

    return derivedMetadata;
}

} // namespace lunalite::asset
