#include "model_asset_definition_writer.h"

#include <cstdint>

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {

void ModelAssetDefinitionWriter::writeSingleMeshDefinition(const std::filesystem::path& modelPath,
                                                           AssetHandle meshHandle,
                                                           const std::vector<AssetHandle>& materialHandles) const
{
    if (std::filesystem::exists(modelPath)) {
        return;
    }

    YAML::Node modelMesh;
    modelMesh["Mesh"] = static_cast<uint64_t>(meshHandle);

    YAML::Node materials{YAML::NodeType::Sequence};
    for (const auto materialHandle : materialHandles) {
        materials.push_back(static_cast<uint64_t>(materialHandle));
    }
    modelMesh["Materials"] = materials;

    YAML::Node meshes{YAML::NodeType::Sequence};
    meshes.push_back(modelMesh);

    YAML::Node model;
    model["Meshes"] = meshes;

    YAML::Node root;
    root["Model"] = model;

    std::ofstream out(modelPath);
    if (!out.is_open()) {
        return;
    }

    out << root;
}

} // namespace lunalite::asset
