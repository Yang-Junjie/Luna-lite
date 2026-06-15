#pragma once

#include "../LunaLite/asset/asset.h"
#include "../LunaLite/asset/asset_types.h"

#include <cstdint>

#include <optional>

namespace lunalite::editor::drag_drop {

inline constexpr const char* AssetPayloadName = "LUNALITE_ASSET";
inline constexpr const char* EntityPayloadName = "LUNALITE_ENTITY";

struct AssetPayload {
    asset::AssetHandle handle{0};
    asset::AssetType type{asset::AssetType::None};
};

struct EntityPayload {
    uint32_t handle{0};
};

void setAssetPayload(const AssetPayload& payload, const char* previewText);
void setEntityPayload(const EntityPayload& payload, const char* previewText);

std::optional<AssetPayload> acceptAssetPayload();
std::optional<EntityPayload> acceptEntityPayload();

bool acceptAssetHandle(asset::AssetType type, asset::AssetHandle& handle);

} // namespace lunalite::editor::drag_drop
