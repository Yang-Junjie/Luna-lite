#pragma once
#include "../../prefab.h"

#include <filesystem>

namespace lunalite::asset {

class PrefabAssetDefinitionWriter final {
public:
    void writeDefinition(const std::filesystem::path& prefabPath,
                         const Prefab& prefab,
                         bool overwriteExisting = false) const;
};

} // namespace lunalite::asset
