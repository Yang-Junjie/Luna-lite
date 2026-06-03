#pragma once
#include "../../asset_metadata.h"

#include <filesystem>
#include <vector>

namespace lunalite::asset {

class AssetMetadataStore;
class MaterialAssetDefinitionWriter;
class ModelAssetDefinitionWriter;

class GltfMeshDerivedAssetGenerator final {
public:
    std::vector<AssetMetadata> generate(const std::filesystem::path& sourceMeshPath,
                                        const AssetMetadata& meshMetadata,
                                        AssetMetadataStore& metadataStore,
                                        const MaterialAssetDefinitionWriter& materialDefinitions,
                                        const ModelAssetDefinitionWriter& modelDefinitions) const;
};

} // namespace lunalite::asset
