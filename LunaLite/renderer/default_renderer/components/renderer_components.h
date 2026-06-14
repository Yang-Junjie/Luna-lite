#pragma once

#include "../../../asset/asset.h"
#include "../../interface/camera.h"

#include <cstdint>

#include <glm/glm.hpp>
#include <limits>
#include <vector>

namespace lunalite::scene {

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
