#pragma once

#include "../../LunaLite/asset/asset.h"
#include "../../LunaLite/scene/entity.h"

#include <filesystem>

namespace lunalite::tooling {

enum class SelectionKind {
    None,
    Entity,
    Asset,
    Folder,
    Project
};

struct Selection {
    SelectionKind kind{SelectionKind::None};
    scene::Entity entity{};
    asset::AssetHandle asset{0};
    std::filesystem::path path;
};

} // namespace lunalite::tooling
