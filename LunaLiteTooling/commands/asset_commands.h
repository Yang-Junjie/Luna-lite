#pragma once

#include "command.h"

#include <optional>
#include <string_view>

namespace lunalite::tooling {

class CommandRegistry;

class CreateSpriteCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class CreateSpriteAnimationClipCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class CreateSpriteAnimatorControllerCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SaveSpriteAnimationClipCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SetMaterialParametersCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SaveMaterialCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SetSpriteParametersCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SaveSpriteCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SetTextureImportSettingsCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SaveTextureImportSettingsCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

inline constexpr std::string_view CreateSpriteCommandId = "asset.create_sprite";
inline constexpr std::string_view CreateSpriteAnimationClipCommandId = "asset.create_sprite_animation_clip";
inline constexpr std::string_view CreateSpriteAnimatorControllerCommandId = "asset.create_sprite_animator_controller";
inline constexpr std::string_view SaveSpriteAnimationClipCommandId = "asset.save_sprite_animation_clip";
inline constexpr std::string_view SetMaterialParametersCommandId = "asset.set_material_parameters";
inline constexpr std::string_view SaveMaterialCommandId = "asset.save_material";
inline constexpr std::string_view SetSpriteParametersCommandId = "asset.set_sprite_parameters";
inline constexpr std::string_view SaveSpriteCommandId = "asset.save_sprite";
inline constexpr std::string_view SetTextureImportSettingsCommandId = "asset.set_texture_import_settings";
inline constexpr std::string_view SaveTextureImportSettingsCommandId = "asset.save_texture_import_settings";
inline constexpr std::string_view SpriteFromTextureFactoryId = "SpriteFromTexture";

std::optional<std::string_view> commandIdForAssetFactory(std::string_view factory_id);
void registerAssetCommands(CommandRegistry& registry);

} // namespace lunalite::tooling
