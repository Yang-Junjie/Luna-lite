#pragma once

#include "../asset/asset.h"

#include <vector>

namespace lunalite::scene {

struct ScriptBinding {
    asset::AssetHandle script{0};
    bool enabled{true};
};

struct ScriptComponent {
    std::vector<ScriptBinding> scripts;
};

} // namespace lunalite::scene
