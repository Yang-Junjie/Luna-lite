#pragma once
#include "../asset/asset.h"
#include "../renderer/interface/camera.h"

#include <cstdint>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <string>
#include <unordered_map>
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
    bool cast_shadow{true};
    uint32_t submesh_start{0};
    uint32_t submesh_count{std::numeric_limits<uint32_t>::max()};
};

struct SpriteRendererComponent {
    asset::AssetHandle sprite{0};
    glm::vec4 color{1.0f};
    int32_t sorting_layer{0};
    int32_t order_in_layer{0};
    bool depth_test{false};
};

struct SpriteAnimatorParameterValue {
    bool bool_value{false};
    float float_value{0.0f};
    int32_t int_value{0};
    bool trigger_value{false};
};

struct SpriteAnimatorComponent {
    asset::AssetHandle controller{0};
    std::string current_state;
    float state_time{0.0f};
    float speed{1.0f};
    bool playing{true};
    std::unordered_map<std::string, SpriteAnimatorParameterValue> parameters;
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
    float cascade_seam_blend{0.0f};
    float cascade_caster_depth_padding{50.0f};
};

struct DirectionalLightComponent {
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    ShadowSettings shadow;
};
} // namespace lunalite::scene
