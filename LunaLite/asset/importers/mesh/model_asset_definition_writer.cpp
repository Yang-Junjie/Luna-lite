#include "model_asset_definition_writer.h"

#include <cstdint>

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {
YAML::Node matrixNode(const glm::mat4& matrix)
{
    YAML::Node node{YAML::NodeType::Sequence};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            node.push_back(matrix[column][row]);
        }
    }
    return node;
}
} // namespace

void ModelAssetDefinitionWriter::writeDefinition(const std::filesystem::path& modelPath,
                                                 const std::vector<ModelMeshDefinition>& meshes,
                                                 bool overwriteExisting) const
{
    if (!overwriteExisting && std::filesystem::exists(modelPath)) {
        return;
    }

    YAML::Node meshNodes{YAML::NodeType::Sequence};
    for (const auto& meshDefinition : meshes) {
        YAML::Node modelMesh;
        modelMesh["Mesh"] = static_cast<uint64_t>(meshDefinition.mesh);
        modelMesh["Transform"] = matrixNode(meshDefinition.transform);

        if (meshDefinition.submesh_start != 0) {
            modelMesh["SubMeshStart"] = meshDefinition.submesh_start;
        }
        if (meshDefinition.submesh_count != std::numeric_limits<uint32_t>::max()) {
            modelMesh["SubMeshCount"] = meshDefinition.submesh_count;
        }

        YAML::Node materials{YAML::NodeType::Sequence};
        for (const auto materialHandle : meshDefinition.materials) {
            materials.push_back(static_cast<uint64_t>(materialHandle));
        }
        modelMesh["Materials"] = materials;

        meshNodes.push_back(modelMesh);
    }

    YAML::Node model;
    model["Meshes"] = meshNodes;

    YAML::Node root;
    root["Model"] = model;

    std::ofstream out(modelPath);
    if (!out.is_open()) {
        return;
    }

    out << root;
}

void ModelAssetDefinitionWriter::writeSingleMeshDefinition(const std::filesystem::path& modelPath,
                                                           AssetHandle meshHandle,
                                                           const std::vector<AssetHandle>& materialHandles) const
{
    writeDefinition(modelPath, {ModelMeshDefinition{.mesh = meshHandle, .materials = materialHandles}});
}

} // namespace lunalite::asset
