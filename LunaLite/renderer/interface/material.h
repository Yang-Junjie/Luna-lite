#pragma once
#include "../../asset/asset.h"

#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
class Material : public asset::Asset {
public:
    enum class ShadingModel {
        Lit = 0,
        Unlit
    };

    ShadingModel shading_model{ShadingModel::Lit};
    glm::vec4 albedo{0.8f, 0.65f, 0.5f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    glm::vec3 emission{0.0f};
    float emission_strength{0.0f};

    asset::AssetHandle albedo_texture{0};
    asset::AssetHandle normal_texture{0};
    asset::AssetHandle metallic_roughness_texture{0};

    asset::AssetType getAssetsType() const override
    {
        return asset::AssetType::Material;
    }
};
} // namespace lunalite::renderer::interface
