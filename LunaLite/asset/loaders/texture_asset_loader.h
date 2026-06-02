#pragma once
#include "../../renderer/interface/texture.h"
#include "../asset_metadata.h"

#include <memory>

namespace lunalite::asset {
class TextureAssetLoader {
public:
    static std::shared_ptr<renderer::interface::Texture> load(const AssetMetadata& metadata);
};
} // namespace lunalite::asset
