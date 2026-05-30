#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/scene_serializer.h"

#include <filesystem>
#include <iostream>

namespace {
template <typename View> size_t countView(const View& view)
{
    size_t count = 0;
    for ([[maybe_unused]] const auto entity : view) {
        ++count;
    }
    return count;
}
} // namespace

int main()
{
    using namespace lunalite;

    scene::Scene scene;
    {
        auto entity = scene.createEntity();
        auto& transform = scene.getComponent<scene::TransformComponent>(entity);
        transform.translation = {1.0f, 2.0f, 3.0f};
        transform.rotation = {0.1f, 0.2f, 0.3f};
        transform.scale = {2.0f, 2.0f, 2.0f};

        auto& mesh = scene.addComponent<scene::MeshComponent>(entity);
        mesh.mesh = asset::AssetHandle{42};
    }

    {
        auto entity = scene.createEntity();
        auto& camera = scene.addComponent<scene::CameraComponent>(entity);
        camera.primary = true;
    }

    {
        auto entity = scene.createEntity();
        auto& light = scene.addComponent<scene::DirectionalLightComponent>(entity);
        light.direction = {0.0f, -1.0f, 0.0f};
        light.diffuse = {0.7f, 0.8f, 0.9f};
    }

    const auto scenePath = std::filesystem::current_path() / "build" / "scene_serializer_test.lunascene";
    if (!scene::SceneSerializer::serialize(scene, scenePath)) {
        std::cerr << "Failed to serialize scene.\n";
        return 1;
    }

    scene::Scene loadedScene;
    if (!scene::SceneSerializer::deserialize(loadedScene, scenePath)) {
        std::cerr << "Failed to deserialize scene.\n";
        return 1;
    }

    const auto transformMeshView =
        loadedScene.getRegistry().view<const scene::TransformComponent, const scene::MeshComponent>();
    if (countView(transformMeshView) != 1) {
        std::cerr << "Unexpected transform mesh entity count.\n";
        return 1;
    }

    const auto cameraView = loadedScene.getRegistry().view<const scene::CameraComponent>();
    if (countView(cameraView) != 1) {
        std::cerr << "Unexpected camera entity count.\n";
        return 1;
    }

    const auto lightView = loadedScene.getRegistry().view<const scene::DirectionalLightComponent>();
    if (countView(lightView) != 1) {
        std::cerr << "Unexpected light entity count.\n";
        return 1;
    }

    std::cout << "SceneSerializer serialized and deserialized a scene.\n";
    return 0;
}
