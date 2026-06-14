#include "../scene/component_registry.h"
#include "../scene/component_registry_adapters.h"
#include "script_component_registry.h"
#include "script_components.h"

namespace lunalite::script {

void registerScriptComponents(scene::ComponentRegistry& registry)
{
    registry.registerComponent({
        .id = ScriptComponentType,
        .display_name = "Script",
        .category = "Scripting",
        .has = &scene::componentHasAdapter<scene::ScriptComponent>,
        .add = &scene::componentAddAdapter<scene::ScriptComponent>,
        .remove = &scene::componentRemoveAdapter<scene::ScriptComponent>,
    });
}

} // namespace lunalite::script
