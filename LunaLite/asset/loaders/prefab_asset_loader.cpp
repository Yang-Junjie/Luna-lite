#include "../../core/log.h"
#include "../asset_metadata.h"
#include "../../project/project_manager.h"
#include "prefab_asset_loader.h"

#include <cstdint>

#include <filesystem>
#include <limits>
#include <unordered_set>
#include <variant>
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

std::vector<uint32_t> computeRoots(const std::vector<PrefabNode>& nodes)
{
    std::vector<bool> referenced(nodes.size(), false);
    for (const auto& node : nodes) {
        for (const auto child : node.children) {
            if (child < referenced.size()) {
                referenced[child] = true;
            }
        }
    }

    std::vector<uint32_t> roots;
    for (uint32_t index = 0; index < nodes.size(); ++index) {
        if (!referenced[index]) {
            roots.push_back(index);
        }
    }
    return roots;
}

std::shared_ptr<Prefab> loadLegacyModel(const AssetMetadata& metadata, const std::filesystem::path& path)
{
    auto prefab = std::make_shared<Prefab>();
    prefab->handle = metadata.Handle;

    try {
        const auto root = YAML::LoadFile(path.string());
        const auto modelNode = root["Model"] ? root["Model"] : root;
        const auto meshesNode = modelNode["Meshes"];
        if (!meshesNode || !meshesNode.IsSequence()) {
            LUNA_CORE_ERROR("Failed to load legacy model '{}': missing Meshes sequence", path.string());
            return nullptr;
        }

        std::vector<PrefabNode> nodes;
        nodes.reserve(meshesNode.size() + 1);

        PrefabNode rootNode;
        rootNode.name = metadata.Name.empty() ? path.stem().string() : metadata.Name;
        for (size_t meshIndex = 0; meshIndex < meshesNode.size(); ++meshIndex) {
            const auto& meshNode = meshesNode[meshIndex];

            PrefabNode node;
            node.name = rootNode.name + "_Mesh_" + std::to_string(meshIndex);
            node.transform = parseMatrix(meshNode["Transform"]);
            node.mesh = AssetHandle{meshNode["Mesh"].as<uint64_t>(0)};
            node.submesh_start = meshNode["SubMeshStart"].as<uint32_t>(0);
            node.submesh_count = meshNode["SubMeshCount"].as<uint32_t>(std::numeric_limits<uint32_t>::max());
            node.materials = parseHandles(meshNode["Materials"]);

            rootNode.children.push_back(static_cast<uint32_t>(nodes.size() + 1));
            nodes.push_back(std::move(node));
        }

        nodes.insert(nodes.begin(), std::move(rootNode));
        prefab->setNodes(std::move(nodes));
        prefab->setRoots({0});
        LUNA_CORE_DEBUG("Loaded legacy prefab '{}' (mesh entries: {}, handle {})",
                        path.string(),
                        prefab->getNodes().size() > 0 ? prefab->getNodes().size() - 1 : 0,
                        metadata.Handle.toString());
        return prefab;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load legacy model '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load legacy model '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}
} // namespace

std::shared_ptr<Prefab> PrefabAssetLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    try {
        const auto root = YAML::LoadFile(path.string());

        if (root["Model"]) {
            return loadLegacyModel(metadata, path);
        }

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

        prefab->setNodes(std::move(nodes));

        std::vector<uint32_t> roots;
        if (const auto rootsNode = prefabNode["Roots"]; rootsNode && rootsNode.IsSequence()) {
            roots.reserve(rootsNode.size());
            for (const auto& rootEntry : rootsNode) {
                roots.push_back(rootEntry.as<uint32_t>(0));
            }
        } else {
            roots = computeRoots(prefab->getNodes());
        }
        prefab->setRoots(std::move(roots));

        LUNA_CORE_DEBUG("Loaded prefab '{}' (nodes: {}, roots: {}, handle {})",
                        path.string(),
                        prefab->getNodes().size(),
                        prefab->getRoots().size(),
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
