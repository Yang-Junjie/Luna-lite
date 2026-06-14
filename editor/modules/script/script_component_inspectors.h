#pragma once

namespace lunalite::scene {
class ComponentRegistry;
}

namespace lunalite::editor {

void registerScriptComponentInspectors(scene::ComponentRegistry& registry);

} // namespace lunalite::editor
