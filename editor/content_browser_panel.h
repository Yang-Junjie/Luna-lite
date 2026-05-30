#pragma once

#include "../LunaLite/asset/asset.h"
#include "../LunaLite/asset/asset_types.h"

namespace lunalite::editor {

inline constexpr const char* AssetDragDropPayloadName = "LUNALITE_ASSET";

struct AssetDragDropPayload {
    asset::AssetHandle handle{0};
    asset::AssetType type{asset::AssetType::None};
};

class ContentBrowserPanel {
public:
    void onImGuiRender();
};

} // namespace lunalite::editor
