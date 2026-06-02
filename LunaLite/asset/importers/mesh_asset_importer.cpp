#include "../../core/log.h"
#include "mesh_asset_importer.h"

#include <cctype>

#include <algorithm>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <tiny_obj_loader.h>
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

    for (auto& c : value) {
        const auto ch = static_cast<unsigned char>(c);
        if (!std::isalnum(ch) && c != '-' && c != '_') {
            c = '_';
        }
    }

    return value;
}

bool hasUsableDiffuse(const tinyobj::material_t& material)
{
    return material.diffuse[0] > 0.0f || material.diffuse[1] > 0.0f || material.diffuse[2] > 0.0f;
}

float materialRoughness(const tinyobj::material_t& material)
{
    return material.roughness > 0.0f ? material.roughness : 0.5f;
}

bool isGltfExtension(const std::filesystem::path& assetPath)
{
    auto extension = assetPath.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".gltf" || extension == ".glb";
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

YAML::Node textureConfig(const fastgltf::Asset& asset, const fastgltf::Texture& texture, const char* colorSpace)
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
    imagePathForTexture(const std::filesystem::path& assetPath, const fastgltf::Asset& asset, size_t textureIndex)
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

    return (assetPath.parent_path() / uri->uri.fspath()).lexically_normal();
}

void writeGltfMaterial(const std::filesystem::path& materialPath,
                       const fastgltf::Material& material,
                       const std::unordered_map<size_t, AssetHandle>& textureHandles)
{
    if (std::filesystem::exists(materialPath)) {
        return;
    }

    const auto& baseColor = material.pbrData.baseColorFactor;
    const auto& emission = material.emissiveFactor;

    std::ofstream out(materialPath);
    out << "Material:\n";
    out << "  ShadingModel: " << (material.unlit ? "Unlit" : "Lit") << "\n";
    out << "  Albedo: [" << baseColor[0] << ", " << baseColor[1] << ", " << baseColor[2] << ", " << baseColor[3]
        << "]\n";
    out << "  Metallic: " << std::clamp(static_cast<float>(material.pbrData.metallicFactor), 0.0f, 1.0f) << "\n";
    out << "  Roughness: " << std::clamp(static_cast<float>(material.pbrData.roughnessFactor), 0.0f, 1.0f) << "\n";
    out << "  Emission: [" << emission[0] << ", " << emission[1] << ", " << emission[2] << "]\n";
    out << "  EmissionStrength: " << static_cast<float>(material.emissiveStrength) << "\n";
    out << "  NormalScale: " << (material.normalTexture ? static_cast<float>(material.normalTexture->scale) : 1.0f)
        << "\n";
    out << "  OcclusionStrength: "
        << (material.occlusionTexture ? static_cast<float>(material.occlusionTexture->strength) : 1.0f) << "\n";

    const auto textureHandle = [&](const auto& info) -> AssetHandle {
        if (!info) {
            return AssetHandle{0};
        }
        const auto it = textureHandles.find(info->textureIndex);
        return it != textureHandles.end() ? it->second : AssetHandle{0};
    };

    const auto albedoTexture = textureHandle(material.pbrData.baseColorTexture);
    const auto normalTexture = textureHandle(material.normalTexture);
    const auto metallicRoughnessTexture = textureHandle(material.pbrData.metallicRoughnessTexture);
    const auto occlusionTexture = textureHandle(material.occlusionTexture);
    const auto emissionTexture = textureHandle(material.emissiveTexture);
    if (albedoTexture.isValid() || normalTexture.isValid() || metallicRoughnessTexture.isValid() ||
        occlusionTexture.isValid() || emissionTexture.isValid()) {
        out << "  Textures:\n";
        if (albedoTexture.isValid()) {
            out << "    Albedo: " << static_cast<uint64_t>(albedoTexture) << "\n";
        }
        if (normalTexture.isValid()) {
            out << "    Normal: " << static_cast<uint64_t>(normalTexture) << "\n";
        }
        if (metallicRoughnessTexture.isValid()) {
            out << "    MetallicRoughness: " << static_cast<uint64_t>(metallicRoughnessTexture) << "\n";
        }
        if (occlusionTexture.isValid()) {
            out << "    Occlusion: " << static_cast<uint64_t>(occlusionTexture) << "\n";
        }
        if (emissionTexture.isValid()) {
            out << "    Emission: " << static_cast<uint64_t>(emissionTexture) << "\n";
        }
    }
}
} // namespace

std::vector<AssetMetadata> MeshAssetImporter::import(const std::filesystem::path& assetPath)
{
    auto metadata = createMetadata(assetPath, AssetType::Mesh);
    const auto metaPath = getMetaFilePath(metadata);

    if (std::filesystem::exists(metaPath)) {
        const auto oldMetadata = deserializeMetadata(metaPath);
        if (oldMetadata.Handle.isValid()) {
            metadata.Handle = oldMetadata.Handle;
        }
        metadata.MemoryOnly = oldMetadata.MemoryOnly;
        metadata.SpecializedConfig = oldMetadata.SpecializedConfig;
    }

    metadata.Type = AssetType::Mesh;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = makeProjectRelative(assetPath);

    if (!serializeMetadata(metadata)) {
        return {};
    }

    auto importedMetadata = std::vector<AssetMetadata>{metadata};
    auto companionMetadata = isGltfExtension(assetPath) ? generateGltfModelCompanions(assetPath, metadata)
                                                        : generateObjModelCompanions(assetPath, metadata);
    importedMetadata.insert(importedMetadata.end(), companionMetadata.begin(), companionMetadata.end());
    return importedMetadata;
}

std::vector<std::string> MeshAssetImporter::getSupportedExtensions() const
{
    return {".obj", ".gltf", ".glb"};
}

AssetMetadata MeshAssetImporter::createOrLoadCompanionMetadata(const std::filesystem::path& assetPath,
                                                               AssetType type,
                                                               AssetHandle suggestedHandle,
                                                               const YAML::Node& config)
{
    auto metadata = createMetadata(assetPath, type);
    const auto metaPath = getMetaFilePath(metadata);
    if (std::filesystem::exists(metaPath)) {
        const auto oldMetadata = deserializeMetadata(metaPath);
        if (oldMetadata.Handle.isValid()) {
            metadata.Handle = oldMetadata.Handle;
        }
        metadata.MemoryOnly = oldMetadata.MemoryOnly;
        metadata.SpecializedConfig = oldMetadata.SpecializedConfig;
    } else if (suggestedHandle.isValid()) {
        metadata.Handle = suggestedHandle;
    }

    metadata.Type = type;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = makeProjectRelative(assetPath);
    if (!hasSpecializedConfig(metadata.SpecializedConfig) && hasSpecializedConfig(config)) {
        metadata.SpecializedConfig = config;
    }
    if (!serializeMetadata(metadata)) {
        return {};
    }

    return metadata;
}

void MeshAssetImporter::writeDefaultMaterial(const std::filesystem::path& materialPath)
{
    if (std::filesystem::exists(materialPath)) {
        return;
    }

    std::ofstream out(materialPath);
    out << "Material:\n";
    out << "  ShadingModel: Lit\n";
    out << "  Albedo: [0.8, 0.65, 0.5, 1.0]\n";
    out << "  Metallic: 0.0\n";
    out << "  Roughness: 0.5\n";
    out << "  Emission: [0.0, 0.0, 0.0]\n";
    out << "  EmissionStrength: 0.0\n";
    out << "  NormalScale: 1.0\n";
    out << "  OcclusionStrength: 1.0\n";
}

void MeshAssetImporter::writeMaterial(const std::filesystem::path& materialPath, const tinyobj::material_t& material)
{
    if (std::filesystem::exists(materialPath)) {
        return;
    }

    const auto albedo = hasUsableDiffuse(material) ? material.diffuse : nullptr;
    std::ofstream out(materialPath);
    out << "Material:\n";
    out << "  ShadingModel: Lit\n";
    if (albedo != nullptr) {
        out << "  Albedo: [" << albedo[0] << ", " << albedo[1] << ", " << albedo[2] << ", 1.0]\n";
    } else {
        out << "  Albedo: [0.8, 0.65, 0.5, 1.0]\n";
    }
    out << "  Metallic: " << std::clamp(static_cast<float>(material.metallic), 0.0f, 1.0f) << "\n";
    out << "  Roughness: " << std::clamp(materialRoughness(material), 0.0f, 1.0f) << "\n";
    out << "  Emission: [" << material.emission[0] << ", " << material.emission[1] << ", " << material.emission[2]
        << "]\n";
    out << "  EmissionStrength: 1.0\n";
    out << "  NormalScale: 1.0\n";
    out << "  OcclusionStrength: 1.0\n";
}

void MeshAssetImporter::writeModel(const std::filesystem::path& modelPath,
                                   AssetHandle meshHandle,
                                   const std::vector<AssetHandle>& materialHandles)
{
    if (std::filesystem::exists(modelPath)) {
        return;
    }

    std::ofstream out(modelPath);
    out << "Model:\n";
    out << "  Meshes:\n";
    out << "    - Mesh: " << static_cast<uint64_t>(meshHandle) << "\n";
    out << "      Materials:\n";
    for (const auto materialHandle : materialHandles) {
        out << "        - " << static_cast<uint64_t>(materialHandle) << "\n";
    }
}

std::vector<AssetMetadata> MeshAssetImporter::generateObjModelCompanions(const std::filesystem::path& assetPath,
                                                                         const AssetMetadata& meshMetadata)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string error;

    std::vector<AssetMetadata> companionMetadata;
    const auto materialBasePath = (assetPath.parent_path() / "").string();
    const bool loaded = tinyobj::LoadObj(
        &attrib, &shapes, &materials, &warn, &error, assetPath.string().c_str(), materialBasePath.c_str(), true);
    if (!loaded && !error.empty()) {
        LUNA_CORE_WARN("Failed to inspect OBJ materials for '{}': {}", assetPath.string(), error);
    }

    std::vector<AssetHandle> materialHandles;
    if (loaded && !materials.empty()) {
        for (size_t i = 0; i < materials.size(); ++i) {
            const auto materialName =
                sanitizeAssetName(materials[i].name.empty() ? "Material" + std::to_string(i) : materials[i].name);
            const auto materialPath =
                assetPath.parent_path() / (assetPath.stem().string() + "_" + materialName + ".lunamat");
            writeMaterial(materialPath, materials[i]);
            const auto materialMetadata = createOrLoadCompanionMetadata(
                materialPath,
                AssetType::Material,
                AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2 + static_cast<uint64_t>(i)});
            if (materialMetadata.Handle.isValid()) {
                materialHandles.push_back(materialMetadata.Handle);
                companionMetadata.push_back(materialMetadata);
            }
        }
    }

    if (materialHandles.empty()) {
        const auto materialPath = assetPath.parent_path() / (assetPath.stem().string() + "_Default.lunamat");
        writeDefaultMaterial(materialPath);
        const auto materialMetadata = createOrLoadCompanionMetadata(
            materialPath, AssetType::Material, AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2});
        if (materialMetadata.Handle.isValid()) {
            materialHandles.push_back(materialMetadata.Handle);
            companionMetadata.push_back(materialMetadata);
        }
    }

    const auto modelPath = assetPath.parent_path() / (assetPath.stem().string() + ".lunamodel");
    writeModel(modelPath, meshMetadata.Handle, materialHandles);
    const auto modelMetadata = createOrLoadCompanionMetadata(
        modelPath, AssetType::Model, AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 1});
    if (modelMetadata.Handle.isValid()) {
        companionMetadata.push_back(modelMetadata);
    }

    return companionMetadata;
}

std::vector<AssetMetadata> MeshAssetImporter::generateGltfModelCompanions(const std::filesystem::path& assetPath,
                                                                          const AssetMetadata& meshMetadata)
{
    auto loadedAsset = loadGltfAsset(assetPath);
    if (!loadedAsset) {
        LUNA_CORE_WARN("Failed to inspect glTF materials for '{}': {}",
                       assetPath.string(),
                       fastgltf::getErrorMessage(loadedAsset.error()));
        return {};
    }

    auto asset = std::move(loadedAsset.get());
    std::vector<AssetMetadata> companionMetadata;
    std::unordered_map<size_t, AssetHandle> textureHandles;
    std::unordered_set<AssetHandle> emittedTextureMetadata;

    const auto createTextureHandle = [&](size_t textureIndex, const char* colorSpace) -> AssetHandle {
        if (const auto it = textureHandles.find(textureIndex); it != textureHandles.end()) {
            return it->second;
        }
        if (textureIndex >= asset.textures.size()) {
            return AssetHandle{0};
        }

        const auto imagePath = imagePathForTexture(assetPath, asset, textureIndex);
        if (imagePath.empty() || !std::filesystem::exists(imagePath)) {
            return AssetHandle{0};
        }

        const auto textureMetadata = createOrLoadCompanionMetadata(
            imagePath,
            AssetType::Texture,
            AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 10'000 + static_cast<uint64_t>(textureIndex)},
            textureConfig(asset, asset.textures[textureIndex], colorSpace));
        if (!textureMetadata.Handle.isValid()) {
            return AssetHandle{0};
        }

        textureHandles.emplace(textureIndex, textureMetadata.Handle);
        if (!emittedTextureMetadata.contains(textureMetadata.Handle)) {
            companionMetadata.push_back(textureMetadata);
            emittedTextureMetadata.insert(textureMetadata.Handle);
        }
        return textureMetadata.Handle;
    };

    std::vector<AssetHandle> materialHandles;
    if (!asset.materials.empty()) {
        for (size_t i = 0; i < asset.materials.size(); ++i) {
            const auto& material = asset.materials[i];
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

            const auto materialName =
                sanitizeAssetName(material.name.empty() ? "Material" + std::to_string(i) : std::string{material.name});
            const auto materialPath =
                assetPath.parent_path() / (assetPath.stem().string() + "_" + materialName + ".lunamat");
            writeGltfMaterial(materialPath, material, textureHandles);
            const auto materialMetadata = createOrLoadCompanionMetadata(
                materialPath,
                AssetType::Material,
                AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2 + static_cast<uint64_t>(i)});
            if (materialMetadata.Handle.isValid()) {
                materialHandles.push_back(materialMetadata.Handle);
                companionMetadata.push_back(materialMetadata);
            }
        }
    }

    if (materialHandles.empty()) {
        const auto materialPath = assetPath.parent_path() / (assetPath.stem().string() + "_Default.lunamat");
        writeDefaultMaterial(materialPath);
        const auto materialMetadata = createOrLoadCompanionMetadata(
            materialPath, AssetType::Material, AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 2});
        if (materialMetadata.Handle.isValid()) {
            materialHandles.push_back(materialMetadata.Handle);
            companionMetadata.push_back(materialMetadata);
        }
    }

    const auto modelPath = assetPath.parent_path() / (assetPath.stem().string() + ".lunamodel");
    writeModel(modelPath, meshMetadata.Handle, materialHandles);
    const auto modelMetadata = createOrLoadCompanionMetadata(
        modelPath, AssetType::Model, AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 1});
    if (modelMetadata.Handle.isValid()) {
        companionMetadata.push_back(modelMetadata);
    }

    return companionMetadata;
}

} // namespace lunalite::asset
