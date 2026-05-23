#include "../LunaLite/asset/asset_database.h"
#include "../LunaLite/asset/mesh_asset_loader.h"
#include "../LunaLite/renderer/interface/mesh.h"
#include "../LunaLite/renderer/path_tracing_renderer/path_tracing_renderer.h"

#include <filesystem>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {
std::filesystem::path findTestMesh()
{
    const std::filesystem::path candidates[] = {
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

    const auto mesh_handle = asset::MeshAssetLoader::loadObj(findTestMesh());
    const auto* mesh = asset::AssetDatabase::get().get<renderer::interface::Mesh>(mesh_handle);
    if (mesh == nullptr) {
        std::cerr << "Failed to load test mesh.\n";
        return 1;
    }

    const std::filesystem::path output_path = "path_tracing_renderer_frame.ppm";
    renderer::PathTracingRenderer renderer(256, 256, output_path);

    const auto camera_pos = glm::vec3(3.0f, 2.0f, 5.0f);
    const auto view = glm::lookAt(camera_pos, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const auto projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    const auto model = glm::scale(glm::mat4{1.0f}, glm::vec3{1.5f});

    renderer.beginFrame();
    renderer.setViewProjection(view, projection, camera_pos);
    renderer.setDirectionalLight(glm::normalize(glm::vec3{-0.5f, -1.0f, -0.3f}),
                                 glm::vec3{0.04f},
                                 glm::vec3{0.9f},
                                 glm::vec3{1.0f});
    renderer.renderMesh(*mesh, model);
    renderer.endFrame();

    std::cout << "Path tracing renderer wrote " << output_path.string() << "\n";
    return 0;
}
