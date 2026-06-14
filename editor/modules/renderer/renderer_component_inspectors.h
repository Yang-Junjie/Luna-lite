#pragma once

namespace lunalite::scene {
class ComponentRegistry;
}

namespace lunalite::editor {

void registerRendererComponentInspectors(scene::ComponentRegistry& registry);

} // namespace lunalite::editor
