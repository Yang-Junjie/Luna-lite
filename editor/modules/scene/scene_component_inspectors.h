#pragma once

namespace lunalite::scene {
class ComponentRegistry;
}

namespace lunalite::editor {

void registerSceneComponentInspectors(scene::ComponentRegistry& registry);

} // namespace lunalite::editor
