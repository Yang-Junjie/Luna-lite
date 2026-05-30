#pragma once
#include "../asset/asset.h"
#include "../renderer/interface/camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

namespace lunalite::scene {
struct TagComponent {
    std::string tag;

    TagComponent() = default;

    explicit TagComponent(std::string tag)
        : tag(std::move(tag))
    {}
};

struct TransformComponent {
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 getTransform() const
    {
        glm::mat4 transform{1.0f};
        transform = glm::translate(transform, translation);
        transform = glm::rotate(transform, rotation.x, {1.0f, 0.0f, 0.0f});
        transform = glm::rotate(transform, rotation.y, {0.0f, 1.0f, 0.0f});
        transform = glm::rotate(transform, rotation.z, {0.0f, 0.0f, 1.0f});
        transform = glm::scale(transform, scale);
        return transform;
    }
};

struct MeshComponent {
    asset::AssetHandle mesh{0};
};

struct ScriptBinding {
    asset::AssetHandle script{0};
    bool enabled{true};
};

struct ScriptComponent {
    std::vector<ScriptBinding> scripts;
};

struct CameraComponent {
    renderer::interface::Camera camera;
    bool primary{true};
};

struct DirectionalLightComponent {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 ambient{0.05f, 0.05f, 0.05f};
    glm::vec3 diffuse{0.8f, 0.8f, 0.8f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};
};
} // namespace lunalite::scene
