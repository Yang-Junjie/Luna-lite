#pragma once
#include "../../asset.h"

#include <filesystem>
#include <vector>

namespace lunalite::asset {

class ModelAssetDefinitionWriter final {
public:
    void writeSingleMeshDefinition(const std::filesystem::path& modelPath,
                                   AssetHandle meshHandle,
                                   const std::vector<AssetHandle>& materialHandles) const;
};

} // namespace lunalite::asset
