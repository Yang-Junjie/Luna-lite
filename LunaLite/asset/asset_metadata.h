
#pragma once
#include "asset.h"
#include "asset_types.h"

#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
struct AssetMetadata {
    AssetHandle Handle{0};
    AssetType Type{AssetType::None};
    std::string Name;

    // relative project root path
    std::filesystem::path FilePath;

    bool MemoryOnly = false;

    YAML::Node SpecializedConfig;
};
} // namespace lunalite::asset
