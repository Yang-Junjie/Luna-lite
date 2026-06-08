#pragma once

#include "../../LunaLite/asset/asset.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

namespace lunalite::tooling {

using CommandValue = std::variant<bool, int64_t, uint64_t, double, std::string, std::filesystem::path, asset::AssetHandle>;

} // namespace lunalite::tooling
