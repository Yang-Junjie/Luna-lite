#pragma once
#include "../../asset.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <tiny_obj_loader.h>
#include <unordered_map>

namespace lunalite::asset {

class MaterialAssetDefinitionWriter final {
public:
    void writeDefaultDefinition(const std::filesystem::path& materialPath) const;
    void writeObjMaterialDefinition(const std::filesystem::path& materialPath,
                                    const tinyobj::material_t& material) const;
    void writeGltfMaterialDefinition(const std::filesystem::path& materialPath,
                                     const fastgltf::Material& material,
                                     const std::unordered_map<size_t, AssetHandle>& textureHandles,
                                     bool overwriteExisting = false) const;
};

} // namespace lunalite::asset
