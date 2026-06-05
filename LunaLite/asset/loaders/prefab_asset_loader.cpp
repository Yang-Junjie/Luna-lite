#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "../asset_metadata.h"
#include "prefab_asset_loader.h"

#include <cstdint>

#include <filesystem>
#include <limits>
#include <string>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {
glm::mat4 parseMatrix(const YAML::Node& node)
{
    glm::mat4 matrix{1.0f};
    if (!node || !node.IsSequence() || node.size() != 16) {
        return matrix;
    }

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            matrix[column][row] = node[static_cast<size_t>(column * 4 + row)].as<float>();
        }
    }
    return matrix;
}

std::vector<AssetHandle> parseHandles(const YAML::Node& node)
{
    std::vector<AssetHandle> handles;
    if (!node || !node.IsSequence()) {
        return handles;
    }

    handles.reserve(node.size());
    for (const auto& entry : node) {
        handles.push_back(AssetHandle{entry.as<uint64_t>(0)});
    }
    return handles;
}

uint32_t parseRootIndex(const YAML::Node& prefabNode, size_t nodeCount, const std::filesystem::path& path)
{
    if (const auto rootNode = prefabNode["Root"]; rootNode) {
        const auto rootIndex = rootNode.as<uint32_t>(Prefab::InvalidRoot);
        if (rootIndex < nodeCount) {
            return rootIndex;
        }

        LUNA_CORE_ERROR("Failed to load prefab '{}': Root index {} is out of range for {} node(s)",
                        path.string(),
                        rootIndex,
                        nodeCount);
        return Prefab::InvalidRoot;
    }

    LUNA_CORE_ERROR("Failed to load prefab '{}': missing Root", path.string());
    return Prefab::InvalidRoot;
}

} // namespace

std::shared_ptr<Prefab> PrefabAssetLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    try {
        const auto root = YAML::LoadFile(path.string());

        const auto prefabNode = root["Prefab"] ? root["Prefab"] : root;
        const auto nodesNode = prefabNode["Nodes"];
        if (!nodesNode || !nodesNode.IsSequence()) {
            LUNA_CORE_ERROR("Failed to load prefab '{}': missing Nodes sequence", path.string());
            return nullptr;
        }

        auto prefab = std::make_shared<Prefab>();
        prefab->handle = metadata.Handle;

        std::vector<PrefabNode> nodes;
        nodes.reserve(nodesNode.size());
        for (const auto& nodeEntry : nodesNode) {
            PrefabNode node;
            node.name = nodeEntry["Name"].as<std::string>("");
            node.transform = parseMatrix(nodeEntry["Transform"]);
            node.mesh = AssetHandle{nodeEntry["Mesh"].as<uint64_t>(0)};
            node.submesh_start = nodeEntry["SubMeshStart"].as<uint32_t>(0);
            node.submesh_count = nodeEntry["SubMeshCount"].as<uint32_t>(std::numeric_limits<uint32_t>::max());
            node.materials = parseHandles(nodeEntry["Materials"]);

            if (const auto childrenNode = nodeEntry["Children"]; childrenNode && childrenNode.IsSequence()) {
                for (const auto& childNode : childrenNode) {
                    node.children.push_back(childNode.as<uint32_t>(0));
                }
            }

            nodes.push_back(std::move(node));
        }

        const auto rootIndex = parseRootIndex(prefabNode, nodes.size(), path);
        if (rootIndex == Prefab::InvalidRoot) {
            return nullptr;
        }

        prefab->setNodes(std::move(nodes));
        prefab->setRoot(rootIndex);

        LUNA_CORE_DEBUG("Loaded prefab '{}' (nodes: {}, root: {}, handle {})",
                        path.string(),
                        prefab->getNodes().size(),
                        prefab->hasRoot() ? std::to_string(prefab->getRoot()) : "none",
                        metadata.Handle.toString());
        return prefab;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load prefab '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load prefab '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}

} // namespace lunalite::asset
