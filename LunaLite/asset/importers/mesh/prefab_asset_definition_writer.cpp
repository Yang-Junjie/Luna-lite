#include "../../../core/log.h"
#include "prefab_asset_definition_writer.h"

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

void PrefabAssetDefinitionWriter::writeDefinition(const std::filesystem::path& prefabPath,
                                                  const Prefab& prefab,
                                                  bool overwriteExisting) const
{
    if (!overwriteExisting && std::filesystem::exists(prefabPath)) {
        return;
    }

    YAML::Node prefabNode;

    YAML::Node nodeList{YAML::NodeType::Sequence};
    for (const auto& node : prefab.getNodes()) {
        YAML::Node nodeEntry;
        if (!node.name.empty()) {
            nodeEntry["Name"] = node.name;
        }
        nodeEntry["Transform"] = matrixNode(node.transform);
        if (node.mesh.isValid()) {
            nodeEntry["Mesh"] = static_cast<uint64_t>(node.mesh);
        }
        if (!node.materials.empty()) {
            YAML::Node materials{YAML::NodeType::Sequence};
            for (const auto materialHandle : node.materials) {
                materials.push_back(static_cast<uint64_t>(materialHandle));
            }
            nodeEntry["Materials"] = materials;
        }
        if (node.submesh_start != 0) {
            nodeEntry["SubMeshStart"] = node.submesh_start;
        }
        if (node.submesh_count != std::numeric_limits<uint32_t>::max()) {
            nodeEntry["SubMeshCount"] = node.submesh_count;
        }
        if (!node.children.empty()) {
            YAML::Node children{YAML::NodeType::Sequence};
            for (const auto child : node.children) {
                children.push_back(child);
            }
            nodeEntry["Children"] = children;
        }
        nodeList.push_back(nodeEntry);
    }
    prefabNode["Nodes"] = nodeList;

    if (prefab.hasRoot()) {
        prefabNode["Root"] = prefab.getRoot();
    }

    YAML::Node root;
    root["Prefab"] = prefabNode;

    std::ofstream out(prefabPath);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open prefab definition for writing: '{}'", prefabPath.string());
        return;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write prefab definition: '{}'", prefabPath.string());
    }
}

} // namespace lunalite::asset
