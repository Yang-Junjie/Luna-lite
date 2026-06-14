#pragma once

#include <string_view>

namespace lunalite::scene {
class ComponentRegistry;
}

namespace lunalite::script {

inline constexpr std::string_view ScriptComponentType = "Script";

void registerScriptComponents(scene::ComponentRegistry& registry);

} // namespace lunalite::script
