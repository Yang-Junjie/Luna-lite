#include "../LunaLite/asset/asset_database.h"
#include "../LunaLite/asset/mesh_asset_loader.h"
#include "../LunaLite/renderer/interface/frame_image.h"
#include "../LunaLite/renderer/interface/mesh.h"
#include "../LunaLite/renderer/soft_rasterization_renderer/soft_rasterization_renderer.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <variant>

namespace {
std::filesystem::path findTestMesh()
{
    const std::filesystem::path candidates[] = {
        "sample_project/Assets/Meshs/stanford-bunny.obj",
        "../sample_project/Assets/Meshs/stanford-bunny.obj",
        "../../sample_project/Assets/Meshs/stanford-bunny.obj",
        "assets/stanford-bunny.obj",
        "../assets/stanford-bunny.obj",
        "../../assets/stanford-bunny.obj",
        "sample_project/Assets/stanford-bunny.obj",
        "../sample_project/Assets/stanford-bunny.obj",
        "../../sample_project/Assets/stanford-bunny.obj",
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

    asset::AssetMetadata metadata;
    metadata.Handle = asset::AssetHandle{};
    metadata.Type = asset::AssetType::Mesh;
    metadata.FilePath = std::filesystem::absolute(findTestMesh());

    const auto mesh = asset::MeshAssetLoader::loadObj(metadata);
    if (!mesh) {
        std::cerr << "Failed to load test mesh.\n";
        return 1;
    }

    renderer::SoftRasterizationRenderer renderer(512, 512);

    const auto camera_pos = glm::vec3(3.0f, 2.0f, 5.0f);
    const auto view = glm::lookAt(camera_pos, glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const auto projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    const auto model = glm::scale(glm::mat4{1.0f}, glm::vec3{8.0f});

    renderer.beginFrame();
    renderer.setViewProjection(view, projection, camera_pos);
    renderer::interface::SceneLighting lighting;
    lighting.directional_light_count = 1;
    lighting.directional_light.direction = glm::normalize(glm::vec3{-0.5f, -1.0f, -0.3f});
    lighting.directional_light.ambient = glm::vec3{0.05f};
    lighting.directional_light.diffuse = glm::vec3{0.8f};
    lighting.directional_light.specular = glm::vec3{1.0f};
    renderer.setSceneLighting(lighting);
    renderer.renderMesh(*mesh, model);
    renderer.endFrame();

    const auto& frame_image = renderer.getFrameImage();
    const auto* cpu_storage = std::get_if<renderer::interface::CpuFrameStorage>(&frame_image.storage);
    if (frame_image.width != 512 || frame_image.height != 512 ||
        frame_image.format != renderer::interface::FrameImageFormat::RGBA8_UNorm ||
        frame_image.color_space != renderer::interface::FrameImageColorSpace::SRGB || cpu_storage == nullptr ||
        cpu_storage->pixels == nullptr || cpu_storage->row_pitch != 512u * 4u) {
        std::cerr << "Soft rasterization renderer produced an invalid frame image.\n";
        return 1;
    }

    std::cout << "Soft rasterization renderer produced a CPU frame image.\n";
    return 0;
}
