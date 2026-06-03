#include "../../../core/log.h"
#include "../../metadata/asset_metadata_store.h"
#include "material_asset_definition_writer.h"
#include "model_asset_definition_writer.h"
#include "obj_mesh_derived_asset_generator.h"

#include <cctype>

#include <filesystem>
#include <string>
#include <tiny_obj_loader.h>

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
    ObjMeshDerivedAssetGenerator::generate(const std::filesystem::path& sourceMeshPath,
                                           const AssetMetadata& meshMetadata,
                                           AssetMetadataStore& metadataStore,
                                           const MaterialAssetDefinitionWriter& materialDefinitions,
                                           const ModelAssetDefinitionWriter& modelDefinitions) const
{
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;

    std::vector<AssetMetadata> derivedMetadata;
    const auto materialBasePath = (sourceMeshPath.parent_path() / "").string();
    const bool loaded = tinyobj::LoadObj(&attributes,
                                         &shapes,
                                         &materials,
                                         &warning,
                                         &error,
                                         sourceMeshPath.string().c_str(),
                                         materialBasePath.c_str(),
                                         true);
    if (!loaded && !error.empty()) {
        LUNA_CORE_WARN("Failed to inspect OBJ materials for '{}': {}", sourceMeshPath.string(), error);
    }

    std::vector<AssetHandle> materialHandles;
    if (loaded && !materials.empty()) {
        for (size_t materialIndex = 0; materialIndex < materials.size(); ++materialIndex) {
            const auto materialName =
                sanitizeAssetName(materials[materialIndex].name.empty() ? "Material" + std::to_string(materialIndex)
                                                                        : materials[materialIndex].name);
            const auto materialPath =
                sourceMeshPath.parent_path() / (sourceMeshPath.stem().string() + "_" + materialName + ".lunamat");
            materialDefinitions.writeObjMaterialDefinition(materialPath, materials[materialIndex]);

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
