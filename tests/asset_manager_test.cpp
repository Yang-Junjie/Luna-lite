#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/asset/builtin/builtin_assets.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/renderer/interface/material.h"
#include "../LunaLite/renderer/interface/mesh.h"
#include "../LunaLite/renderer/interface/model.h"

#include <filesystem>
#include <iostream>

namespace {
std::filesystem::path findTestMesh()
{
    const std::filesystem::path candidates[] = {
        "sample_project/Assets/Meshs/cube.obj",
        "../sample_project/Assets/Meshs/cube.obj",
        "../../sample_project/Assets/Meshs/cube.obj",
        "sample_project/Assets/cube.obj",
        "../sample_project/Assets/cube.obj",
        "../../sample_project/Assets/cube.obj",
        "assets/cube.obj",
        "../assets/cube.obj",
        "../../assets/cube.obj",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return candidates[0];
}
} // namespace

int main()
{
    using namespace lunalite;

    const auto sourceMesh = findTestMesh();
    if (!std::filesystem::exists(sourceMesh)) {
        std::cerr << "Failed to find test mesh.\n";
        return 1;
    }

    const auto projectRoot = std::filesystem::current_path() / "build" / "asset_manager_test_project";
    std::error_code error;
    std::filesystem::remove_all(projectRoot, error);

    project::ProjectInfo info;
    info.name = "AssetManagerTestProject";
    info.assets_path = "Assets";
    if (!project::ProjectManager::instance().createProject(projectRoot, info)) {
        std::cerr << "Failed to create test project.\n";
        return 1;
    }

    const auto targetMesh = projectRoot / info.assets_path / "cube.obj";
    std::filesystem::copy_file(sourceMesh, targetMesh, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::cerr << "Failed to copy test mesh: " << error.message() << "\n";
        return 1;
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        std::cerr << "Failed to load project assets.\n";
        return 1;
    }

    const auto handle = asset::AssetManager::get().getHandleByRelativePath("Assets/cube.obj");
    if (!handle.isValid()) {
        std::cerr << "Failed to resolve imported mesh handle.\n";
        return 1;
    }

    const auto fileNameHandle = asset::AssetManager::get().getHandleByFileName("cube.obj");
    if (!fileNameHandle.isValid() || fileNameHandle != handle) {
        std::cerr << "Failed to resolve imported mesh by file name.\n";
        return 1;
    }

    const auto* mesh = asset::AssetManager::get().getAsset<renderer::interface::Mesh>(handle);
    if (mesh == nullptr || mesh->getSubMeshes().empty()) {
        std::cerr << "Failed to load mesh through AssetManager.\n";
        return 1;
    }
    const auto& submesh = mesh->getSubMeshes().front();
    if (submesh.getVertices().empty() || submesh.getIndices().empty()) {
        std::cerr << "Failed to load submesh geometry through AssetManager.\n";
        return 1;
    }

    const auto materialHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/cube_Default.lunamat");
    const auto* material = asset::AssetManager::get().getAsset<renderer::interface::Material>(materialHandle);
    if (material == nullptr || material->parameters->albedo.r < 0.7f) {
        std::cerr << "Failed to load generated material through AssetManager.\n";
        return 1;
    }

    const auto* defaultMaterial =
        asset::AssetManager::get().getAsset<renderer::interface::Material>(asset::builtin::defaultMaterialHandle());
    const auto* cubeMesh =
        asset::AssetManager::get().getAsset<renderer::interface::Mesh>(asset::builtin::cubeMeshHandle());
    const auto* cubeModel =
        asset::AssetManager::get().getAsset<renderer::interface::Model>(asset::builtin::cubeModelHandle());
    if (defaultMaterial == nullptr || defaultMaterial->parameters == nullptr || cubeMesh == nullptr ||
        cubeMesh->getSubMeshes().empty() || cubeModel == nullptr || cubeModel->getMeshes().empty()) {
        std::cerr << "Failed to load built-in mesh, material, and model assets.\n";
        return 1;
    }

    const auto modelHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/cube.lunamodel");
    const auto* model = asset::AssetManager::get().getAsset<renderer::interface::Model>(modelHandle);
    if (model == nullptr || model->getMeshes().empty() || model->getMeshes().front().materials.empty()) {
        std::cerr << "Failed to load model through AssetManager.\n";
        return 1;
    }

    std::cout << "AssetManager imported and loaded mesh, material, and model assets.\n";
    return 0;
}
