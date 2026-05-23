#include "../LunaLite/asset/asset_database.h"
#include "../LunaLite/asset/mesh_asset_loader.h"
#include "../LunaLite/renderer/interface/mesh.h"
#include "../LunaLite/renderer/soft_rasterization_renderer/soft_rasterization_renderer.h"

#include <filesystem>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {
std::filesystem::path findTestMesh()
{
    const std::filesystem::path candidates[] = {
        "assets/stanford-bunny.obj",
        "../assets/stanford-bunny.obj",
        "../../assets/stanford-bunny.obj",
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

    const auto mesh_handle = asset::MeshAssetLoader::loadObj(findTestMesh());
    const auto* mesh = asset::AssetDatabase::get().get<renderer::interface::Mesh>(mesh_handle);
    if (mesh == nullptr) {
        std::cerr << "Failed to load test mesh.\n";
        return 1;
    }

    const std::filesystem::path output_path = "soft_rasterization_renderer_frame.ppm";
    renderer::SoftRasterizationRenderer renderer(512, 512, output_path);

    const auto camera_pos = glm::vec3(3.0f, 2.0f, 5.0f);
    const auto view = glm::lookAt(camera_pos, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const auto projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    const auto model = glm::scale(glm::mat4{1.0f}, glm::vec3{8.0f});

    renderer.beginFrame();
    renderer.setViewProjection(view, projection, camera_pos);
    renderer.setDirectionalLight(glm::normalize(glm::vec3{-0.5f, -1.0f, -0.3f}),
                                 glm::vec3{0.05f},
                                 glm::vec3{0.8f},
                                 glm::vec3{1.0f});
    renderer.renderMesh(*mesh, model);
    renderer.endFrame();

    std::cout << "Tiny renderer wrote " << output_path.string() << "\n";
    return 0;
}
