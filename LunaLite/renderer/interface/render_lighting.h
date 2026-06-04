#pragma once
#include "../../asset/asset.h"

#include <cstdint>
#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
struct RenderDirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 radiance{0.0f};
};

struct RenderLighting {
    uint32_t directional_light_count{0};
    RenderDirectionalLight directional_light{};
    asset::AssetHandle environment_map{0};
    float environment_intensity{1.0f};
};
} // namespace lunalite::renderer::interface
