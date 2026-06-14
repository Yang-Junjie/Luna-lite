#include "editor_component_inspectors.h"
#include "modules/renderer/renderer_component_inspectors.h"
#include "modules/scene/scene_component_inspectors.h"
#include "modules/script/script_component_inspectors.h"

namespace lunalite::editor {

void registerEditorComponentInspectors(scene::ComponentRegistry& registry)
{
    registerSceneComponentInspectors(registry);
    registerRendererComponentInspectors(registry);
    registerScriptComponentInspectors(registry);
}

} // namespace lunalite::editor
