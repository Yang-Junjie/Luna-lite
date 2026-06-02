#include "../../core/log.h"
#include "mesh_asset_importer.h"

#include <cctype>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <tiny_obj_loader.h>
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
} // namespace

AssetMetadata MeshAssetImporter::import(const std::filesystem::path& assetPath)
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

    generateModelCompanions(assetPath, metadata);
    return metadata;
}

std::vector<std::string> MeshAssetImporter::getSupportedExtensions() const
{
    return {".obj"};
}

AssetMetadata MeshAssetImporter::createOrLoadCompanionMetadata(const std::filesystem::path& assetPath,
                                                               AssetType type,
                                                               AssetHandle suggestedHandle)
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

void MeshAssetImporter::generateModelCompanions(const std::filesystem::path& assetPath,
                                                const AssetMetadata& meshMetadata)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string error;

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
        }
    }

    const auto modelPath = assetPath.parent_path() / (assetPath.stem().string() + ".lunamodel");
    writeModel(modelPath, meshMetadata.Handle, materialHandles);
    createOrLoadCompanionMetadata(
        modelPath, AssetType::Model, AssetHandle{static_cast<uint64_t>(meshMetadata.Handle) + 1});
}

} // namespace lunalite::asset
