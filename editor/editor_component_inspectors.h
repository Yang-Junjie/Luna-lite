#pragma once

namespace lunalite::scene {
class ComponentRegistry;
}

namespace lunalite::editor {

void registerEditorComponentInspectors(scene::ComponentRegistry& registry);

} // namespace lunalite::editor
