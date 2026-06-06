#include "../LunaLite/scene/components.h"
#include "../LunaLite/scene/scene.h"
#include "../LunaLite/scene/scene_serializer.h"

#include <cmath>

#include <filesystem>
#include <glm/geometric.hpp>
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

bool nearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= 0.001f;
}

bool nearlyEqual(const glm::mat4& lhs, const glm::mat4& rhs)
{
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!nearlyEqual(lhs[column][row], rhs[column][row])) {
                return false;
            }
        }
    }

    return true;
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

        auto& meshRenderer = scene.addComponent<scene::MeshRendererComponent>(entity);
        meshRenderer.mesh = asset::AssetHandle{42};
        meshRenderer.materials.push_back(asset::AssetHandle{84});

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
        light.shadow.cascade_seam_blend = 5.0f;
    }

    const auto parentEntity = scene.createEntity();
    {
        auto& tag = scene.getComponent<scene::TagComponent>(parentEntity);
        tag.tag = "Parent";
        auto& transform = scene.getComponent<scene::TransformComponent>(parentEntity);
        transform.translation = {4.0f, 5.0f, 6.0f};
        transform.rotation = glm::quat{glm::vec3{0.0f, glm::radians(90.0f), 0.0f}};
    }

    const auto childEntity = scene.createEntity();
    glm::mat4 childWorldBefore{1.0f};
    {
        auto& tag = scene.getComponent<scene::TagComponent>(childEntity);
        tag.tag = "Child";
        auto& transform = scene.getComponent<scene::TransformComponent>(childEntity);
        transform.translation = {1.0f, 2.0f, 3.0f};
        transform.rotation = glm::quat{glm::vec3{0.1f, 0.2f, 0.3f}};
        childWorldBefore = transform.getTransform();
    }
    if (!scene.setParent(childEntity, parentEntity, true)) {
        std::cerr << "Failed to parent test entity.\n";
        return 1;
    }
    if (!nearlyEqual(scene.getWorldTransform(childEntity), childWorldBefore)) {
        std::cerr << "Parenting changed the child world transform.\n";
        return 1;
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
        loadedScene.getRegistry().view<const scene::TransformComponent, const scene::MeshRendererComponent>();
    if (countView(transformMeshView) != 1) {
        std::cerr << "Unexpected transform mesh renderer entity count.\n";
        return 1;
    }
    const auto& loadedMeshRenderer =
        transformMeshView.get<const scene::MeshRendererComponent>(*transformMeshView.begin());
    if (loadedMeshRenderer.mesh != asset::AssetHandle{42} || loadedMeshRenderer.materials.size() != 1 ||
        loadedMeshRenderer.materials.front() != asset::AssetHandle{84}) {
        std::cerr << "Unexpected mesh renderer component data.\n";
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

    const auto parentView = loadedScene.getRegistry().view<const scene::ParentComponent>();
    if (countView(parentView) != 1) {
        std::cerr << "Unexpected parent component count.\n";
        return 1;
    }

    scene::Entity loadedParent{};
    scene::Entity loadedChild{};
    for (const auto entity : loadedScene.getRegistry().view<const scene::TagComponent>()) {
        const auto& tag = loadedScene.getComponent<scene::TagComponent>(scene::Entity{entity});
        if (tag.tag == "Parent") {
            loadedParent = scene::Entity{entity};
        } else if (tag.tag == "Child") {
            loadedChild = scene::Entity{entity};
        }
    }
    if (!loadedParent || !loadedChild) {
        std::cerr << "Failed to find serialized hierarchy entities.\n";
        return 1;
    }
    if (loadedScene.getParent(loadedChild).getHandle() != loadedParent.getHandle()) {
        std::cerr << "Failed to deserialize parent relation.\n";
        return 1;
    }
    if (!nearlyEqual(loadedScene.getWorldTransform(loadedChild), childWorldBefore)) {
        std::cerr << "Failed to preserve child world transform through serialization.\n";
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
    if (loadedLight.color != glm::vec3{0.7f, 0.8f, 0.9f} || loadedLight.intensity != 3.0f ||
        loadedLight.shadow.cascade_seam_blend != 5.0f) {
        std::cerr << "Unexpected directional light parameters.\n";
        return 1;
    }

    std::cout << "SceneSerializer serialized and deserialized a scene.\n";
    return 0;
}
