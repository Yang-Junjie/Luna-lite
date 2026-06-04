#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "model_asset_loader.h"

#include <filesystem>
#include <glm/mat4x4.hpp>
#include <limits>
#include <utility>
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
} // namespace

std::shared_ptr<renderer::interface::Model> ModelAssetLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    try {
        const auto root = YAML::LoadFile(path.string());
        const auto modelNode = root["Model"] ? root["Model"] : root;
        const auto meshesNode = modelNode["Meshes"];
        if (!meshesNode || !meshesNode.IsSequence()) {
            LUNA_CORE_ERROR("Failed to load model '{}': missing Meshes sequence", path.string());
            return nullptr;
        }

        std::vector<renderer::interface::ModelMesh> meshes;
        for (const auto& meshNode : meshesNode) {
            renderer::interface::ModelMesh modelMesh;
            modelMesh.mesh = AssetHandle{meshNode["Mesh"].as<uint64_t>(0)};
            modelMesh.transform = parseMatrix(meshNode["Transform"]);
            modelMesh.submesh_start = meshNode["SubMeshStart"].as<uint32_t>(0);
            modelMesh.submesh_count = meshNode["SubMeshCount"].as<uint32_t>(std::numeric_limits<uint32_t>::max());

            if (const auto materialsNode = meshNode["Materials"]; materialsNode && materialsNode.IsSequence()) {
                for (const auto& materialNode : materialsNode) {
                    modelMesh.materials.push_back(AssetHandle{materialNode.as<uint64_t>(0)});
                }
            }

            meshes.push_back(std::move(modelMesh));
        }

        auto model = std::make_shared<renderer::interface::Model>();
        model->handle = metadata.Handle;
        model->setMeshes(std::move(meshes));
        return model;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load model '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load model '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}

} // namespace lunalite::asset
