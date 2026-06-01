#pragma once
#include "../renderer/interface/material.h"
#include "asset_metadata.h"

#include <memory>

namespace lunalite::asset {
class MaterialAssetLoader {
public:
    static std::shared_ptr<renderer::interface::Material> load(const AssetMetadata& metadata);
};
} // namespace lunalite::asset
