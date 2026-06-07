#pragma once
#include "asset_factory.h"

namespace lunalite::asset {

class SpriteAssetFactory final : public AssetFactory {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    AssetType outputType() const override;
    bool canCreate(const AssetFactoryContext& context) const override;
    AssetFactoryResult create(const AssetFactoryContext& context) override;
};

} // namespace lunalite::asset
