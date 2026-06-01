#include "../core/log.h"
#include "../project/project_manager.h"
#include "model_asset_loader.h"

#include <filesystem>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {

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
