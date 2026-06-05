#pragma once
#include "../../asset_metadata.h"

#include <filesystem>
#include <vector>

namespace lunalite::asset {

class AssetMetadataStore;
class MaterialAssetDefinitionWriter;
class PrefabAssetDefinitionWriter;

class ObjMeshDerivedAssetGenerator final {
public:
    std::vector<AssetMetadata> generate(const std::filesystem::path& sourceMeshPath,
                                        const AssetMetadata& meshMetadata,
                                        AssetMetadataStore& metadataStore,
                                        const MaterialAssetDefinitionWriter& materialDefinitions,
                                        const PrefabAssetDefinitionWriter& prefabDefinitions) const;
};

} // namespace lunalite::asset
