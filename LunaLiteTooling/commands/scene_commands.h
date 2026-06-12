#pragma once

#include "command.h"

#include <string_view>

namespace lunalite::tooling {

class CommandRegistry;

class CreateEntityCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class DeleteEntityCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SetParentCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class ClearParentCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class CreateEntityFromAssetCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class CreateBuiltinMeshEntityCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class AddComponentCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class RemoveComponentCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class AddMaterialSlotCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class RemoveMaterialSlotCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class AddScriptBindingCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class RemoveScriptBindingCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditTagCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditTransformCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditMeshRendererCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditSpriteRendererCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditScriptCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditCameraCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditDirectionalLightCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class EditSceneSettingsCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    bool canUndo() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

inline constexpr std::string_view CreateEntityCommandId = "scene.create_entity";
inline constexpr std::string_view DeleteEntityCommandId = "scene.delete_entity";
inline constexpr std::string_view SetParentCommandId = "scene.set_parent";
inline constexpr std::string_view ClearParentCommandId = "scene.clear_parent";
inline constexpr std::string_view CreateEntityFromAssetCommandId = "scene.create_entity_from_asset";
inline constexpr std::string_view CreateBuiltinMeshEntityCommandId = "scene.create_builtin_mesh_entity";
inline constexpr std::string_view AddComponentCommandId = "scene.add_component";
inline constexpr std::string_view RemoveComponentCommandId = "scene.remove_component";
inline constexpr std::string_view AddMaterialSlotCommandId = "scene.add_material_slot";
inline constexpr std::string_view RemoveMaterialSlotCommandId = "scene.remove_material_slot";
inline constexpr std::string_view AddScriptBindingCommandId = "scene.add_script_binding";
inline constexpr std::string_view RemoveScriptBindingCommandId = "scene.remove_script_binding";
inline constexpr std::string_view EditTagCommandId = "scene.edit_tag";
inline constexpr std::string_view EditTransformCommandId = "scene.edit_transform";
inline constexpr std::string_view EditMeshRendererCommandId = "scene.edit_mesh_renderer";
inline constexpr std::string_view EditSpriteRendererCommandId = "scene.edit_sprite_renderer";
inline constexpr std::string_view EditScriptCommandId = "scene.edit_script";
inline constexpr std::string_view EditCameraCommandId = "scene.edit_camera";
inline constexpr std::string_view EditDirectionalLightCommandId = "scene.edit_directional_light";
inline constexpr std::string_view EditSceneSettingsCommandId = "scene.edit_scene_settings";

inline constexpr std::string_view MeshRendererComponentType = "MeshRenderer";
inline constexpr std::string_view SpriteRendererComponentType = "SpriteRenderer";
inline constexpr std::string_view ScriptComponentType = "Script";
inline constexpr std::string_view CameraComponentType = "Camera";
inline constexpr std::string_view DirectionalLightComponentType = "DirectionalLight";

void registerSceneCommands(CommandRegistry& registry);

} // namespace lunalite::tooling
