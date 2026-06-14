#include "../../../LunaLite/scene/component_registry.h"
#include "../../../LunaLite/script/script_component_registry.h"
#include "../../../LunaLite/script/script_components.h"
#include "../../../LunaLiteTooling/commands/scene_commands.h"
#include "../../component_inspector_widgets.h"
#include "../../editor_actions.h"
#include "script_component_inspectors.h"

#include <cstddef>
#include <cstdint>

#include <imgui.h>

namespace lunalite::editor {
namespace {
bool drawScriptInspector(scene::Scene& scene, scene::Entity selectedEntity)
{
    if (!scene.hasComponent<scene::ScriptComponent>(selectedEntity)) {
        return false;
    }

    bool changed = false;
    auto& script = scene.getComponent<scene::ScriptComponent>(selectedEntity);
    if (ImGui::Button("Add Script")) {
        changed |= actions::addScriptBinding(scene, selectedEntity);
    }

    int scriptToDelete = -1;
    for (size_t i = 0; i < script.scripts.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Separator();
        ImGui::Text("Script %zu", i);
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete##ScriptBindingDelete")) {
            scriptToDelete = static_cast<int>(i);
        }

        auto& binding = script.scripts[i];
        auto enabled = binding.enabled;
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditScriptCommandId,
            [&]() {
                return ImGui::Checkbox("Enabled", &enabled);
            },
            [&]() {
                binding.enabled = enabled;
            });
        uint64_t scriptHandle = static_cast<uint64_t>(binding.script);
        changed |= drawLiveSceneEdit(
            scene,
            actions::EditScriptCommandId,
            [&]() {
                return ImGui::InputScalar("Handle", ImGuiDataType_U64, &scriptHandle);
            },
            [&]() {
                binding.script = asset::AssetHandle{scriptHandle};
            });
        auto droppedScript = binding.script;
        if (acceptAssetHandleDrop(asset::AssetType::Script, droppedScript)) {
            if (actions::beginSceneEdit(scene, actions::EditScriptCommandId)) {
                binding.script = droppedScript;
                actions::commitSceneEdit(scene);
            }
            changed = true;
        }
        ImGui::PopID();
    }

    if (scriptToDelete >= 0) {
        changed |= actions::removeScriptBinding(scene, selectedEntity, static_cast<size_t>(scriptToDelete));
    }

    return changed;
}
} // namespace

void registerScriptComponentInspectors(scene::ComponentRegistry& registry)
{
    registry.setInspector(script::ScriptComponentType, &drawScriptInspector);
}

} // namespace lunalite::editor
