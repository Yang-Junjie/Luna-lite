#pragma once
#include "../../asset_importer.h"
#include "gltf_mesh_derived_asset_generator.h"
#include "material_asset_definition_writer.h"
#include "obj_mesh_derived_asset_generator.h"
#include "prefab_asset_definition_writer.h"

namespace lunalite::asset {
class MeshAssetImporter final : public Importer {
public:
    std::vector<AssetMetadata> import(const std::filesystem::path& assetPath,
                                      AssetMetadataStore& metadataStore) override;
    std::vector<std::string> getSupportedExtensions() const override;
    bool shouldRefreshExistingMetadata(const AssetMetadata& metadata,
                                       const std::filesystem::path& assetPath) const override;

private:
    MaterialAssetDefinitionWriter m_materialDefinitions;
    PrefabAssetDefinitionWriter m_prefabDefinitions;
    ObjMeshDerivedAssetGenerator m_objDerivedAssets;
    GltfMeshDerivedAssetGenerator m_gltfDerivedAssets;
};
} // namespace lunalite::asset
