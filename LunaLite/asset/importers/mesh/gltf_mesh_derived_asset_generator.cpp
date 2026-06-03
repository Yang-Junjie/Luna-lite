#include "../../../core/log.h"
#include "../../metadata/asset_metadata_store.h"
#include "gltf_mesh_derived_asset_generator.h"
#include "material_asset_definition_writer.h"
#include "model_asset_definition_writer.h"

#include <cctype>

#include <algorithm>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
                sourceMeshPath.parent_path() / (sourceMeshPath.stem().string() + "_" + materialName + ".lunamat");
            materialDefinitions.writeGltfMaterialDefinition(materialPath, material, textureHandles);

            const auto materialMetadata = createDerivedMetadataFile(
                metadataStore,
                materialPath,
                AssetType::Material,
                AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2 + static_cast<uint64_t>(materialIndex)});
            if (!materialMetadata.Handle.isValid()) {
                continue;
            }

            materialHandles.push_back(materialMetadata.Handle);
            derivedMetadata.push_back(materialMetadata);
        }
    }

    if (materialHandles.empty()) {
        const auto materialPath = sourceMeshPath.parent_path() / (sourceMeshPath.stem().string() + "_Default.lunamat");
        materialDefinitions.writeDefaultDefinition(materialPath);

        const auto materialMetadata =
            createDerivedMetadataFile(metadataStore,
                                      materialPath,
                                      AssetType::Material,
                                      AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2});
        if (materialMetadata.Handle.isValid()) {
            materialHandles.push_back(materialMetadata.Handle);
            derivedMetadata.push_back(materialMetadata);
        }
    }

    const auto modelPath = sourceMeshPath.parent_path() / (sourceMeshPath.stem().string() + ".lunamodel");
    modelDefinitions.writeSingleMeshDefinition(modelPath, meshMetadata.Handle, materialHandles);

    const auto modelMetadata = createDerivedMetadataFile(
        metadataStore, modelPath, AssetType::Model, AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 1});
    if (modelMetadata.Handle.isValid()) {
        derivedMetadata.push_back(modelMetadata);
    }

    return derivedMetadata;
}

} // namespace lunalite::asset
