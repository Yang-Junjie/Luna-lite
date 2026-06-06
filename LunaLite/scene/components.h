#pragma once
#include "../asset/asset.h"
#include "../renderer/interface/camera.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
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
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 getTransform() const
    {
        glm::mat4 transform{1.0f};
        transform = glm::translate(transform, translation);
        transform *= glm::mat4_cast(rotation);
        transform = glm::scale(transform, scale);
        return transform;
    }
};

struct MeshRendererComponent {
    asset::AssetHandle mesh{0};
    std::vector<asset::AssetHandle> materials;
    uint32_t submesh_start{0};
    uint32_t submesh_count{std::numeric_limits<uint32_t>::max()};
};

struct ParentComponent {
    entt::entity parent{entt::null};
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

struct ShadowSettings {
    bool enabled{true};
    uint32_t map_size{2'048};
    float max_distance{80.0f};
    float bias{0.0015f};
    float normal_bias{0.02f};
    uint32_t pcf_radius{1};
    uint32_t cascade_count{1};
    float cascade_split_lambda{0.5f};
};

struct DirectionalLightComponent {
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    ShadowSettings shadow;
};
} // namespace lunalite::scene
