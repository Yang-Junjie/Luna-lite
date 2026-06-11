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
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class DeleteEntityCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SetParentCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class ClearParentCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class CreateEntityFromAssetCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

inline constexpr std::string_view CreateEntityCommandId = "scene.create_entity";
inline constexpr std::string_view DeleteEntityCommandId = "scene.delete_entity";
inline constexpr std::string_view SetParentCommandId = "scene.set_parent";
inline constexpr std::string_view ClearParentCommandId = "scene.clear_parent";
inline constexpr std::string_view CreateEntityFromAssetCommandId = "scene.create_entity_from_asset";

void registerSceneCommands(CommandRegistry& registry);

} // namespace lunalite::tooling
