#pragma once
#include "../core/uuid.h"
#include "asset_types.h"

namespace lunalite::asset {
using AssetHandle = core::UUID;

class Asset {
public:
    AssetHandle handle{0};

    virtual ~Asset() = default;

    virtual AssetType getAssetsType() const
    {
        return AssetType::None;
    }

    virtual bool operator==(const Asset& other) const
    {
        return handle == other.handle;
    }

    virtual bool operator!=(const Asset& other) const
    {
        return !(*this == other);
    }
};
} // namespace lunalite::asset
