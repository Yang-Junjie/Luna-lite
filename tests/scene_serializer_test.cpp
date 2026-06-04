#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/scene_serializer.h"

#include <filesystem>
#include <glm/gtc/quaternion.hpp>
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
    scene.getSettings().environment_map = asset::AssetHandle{126};
    scene.getSettings().environment_intensity = 2.0f;
    {
        auto entity = scene.createEntity();
        auto& transform = scene.getComponent<scene::TransformComponent>(entity);
        transform.translation = {1.0f, 2.0f, 3.0f};
        transform.rotation = glm::quat{glm::vec3{0.1f, 0.2f, 0.3f}};
        transform.scale = {2.0f, 2.0f, 2.0f};

        auto& model = scene.addComponent<scene::ModelComponent>(entity);
        model.model = asset::AssetHandle{42};

        auto& script = scene.addComponent<scene::ScriptComponent>(entity);
        script.scripts.push_back({asset::AssetHandle{84}, true});
    }

    {
        auto entity = scene.createEntity();
        auto& camera = scene.addComponent<scene::CameraComponent>(entity);
        camera.primary = true;
        camera.camera.setExposure(2.5f);
    }

    {
        auto entity = scene.createEntity();
        auto& light = scene.addComponent<scene::DirectionalLightComponent>(entity);
        light.color = {0.7f, 0.8f, 0.9f};
        light.intensity = 3.0f;
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

    const auto transformModelView =
        loadedScene.getRegistry().view<const scene::TransformComponent, const scene::ModelComponent>();
    if (countView(transformModelView) != 1) {
        std::cerr << "Unexpected transform model entity count.\n";
        return 1;
    }

    const auto scriptView = loadedScene.getRegistry().view<const scene::ScriptComponent>();
    if (countView(scriptView) != 1) {
        std::cerr << "Unexpected script entity count.\n";
        return 1;
    }

    const auto cameraView = loadedScene.getRegistry().view<const scene::CameraComponent>();
    if (countView(cameraView) != 1) {
        std::cerr << "Unexpected camera entity count.\n";
        return 1;
    }
    const auto& loadedCamera = cameraView.get<const scene::CameraComponent>(*cameraView.begin());
    if (loadedCamera.camera.getExposure() != 2.5f) {
        std::cerr << "Unexpected camera exposure.\n";
        return 1;
    }

    const auto lightView = loadedScene.getRegistry().view<const scene::DirectionalLightComponent>();
    if (countView(lightView) != 1) {
        std::cerr << "Unexpected light entity count.\n";
        return 1;
    }

    if (loadedScene.getSettings().environment_map != asset::AssetHandle{126}) {
        std::cerr << "Unexpected scene environment map.\n";
        return 1;
    }

    if (loadedScene.getSettings().environment_intensity != 2.0f) {
        std::cerr << "Unexpected scene environment intensity.\n";
        return 1;
    }

    const auto& loadedLight = lightView.get<const scene::DirectionalLightComponent>(*lightView.begin());
    if (loadedLight.color != glm::vec3{0.7f, 0.8f, 0.9f} || loadedLight.intensity != 3.0f) {
        std::cerr << "Unexpected directional light parameters.\n";
        return 1;
    }

    std::cout << "SceneSerializer serialized and deserialized a scene.\n";
    return 0;
}
