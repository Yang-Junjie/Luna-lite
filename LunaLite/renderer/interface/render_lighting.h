#pragma once
#include "../../asset/asset.h"

#include <cstdint>

#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
struct RenderShadowSettings {
    bool enabled{false};
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

struct RenderDirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 radiance{0.0f};
    RenderShadowSettings shadow;
};

struct RenderLighting {
    uint32_t directional_light_count{0};
    RenderDirectionalLight directional_light{};
    asset::AssetHandle environment_map{0};
    float environment_intensity{1.0f};
};
} // namespace lunalite::renderer::interface
