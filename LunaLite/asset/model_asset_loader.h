#pragma once
#include "../renderer/interface/model.h"
#include "asset_metadata.h"

#include <memory>

namespace lunalite::asset {
class ModelAssetLoader {
public:
    static std::shared_ptr<renderer::interface::Model> load(const AssetMetadata& metadata);
};
} // namespace lunalite::asset
