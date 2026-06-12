#pragma once

#include "command.h"

#include <string_view>

namespace lunalite::tooling {

class CommandRegistry;

class CreateProjectCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class OpenProjectCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SaveProjectCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class CreateSceneFileCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class OpenSceneFileCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

class SaveSceneFileCommand final : public Command {
public:
    std::string_view id() const override;
    std::string_view label() const override;
    std::string_view category() const override;
    CommandResult execute(ToolContext& context, const CommandArgs& args) override;
};

inline constexpr std::string_view CreateProjectCommandId = "project.create";
inline constexpr std::string_view OpenProjectCommandId = "project.open";
inline constexpr std::string_view SaveProjectCommandId = "project.save";
inline constexpr std::string_view CreateSceneFileCommandId = "scene.create_file";
inline constexpr std::string_view OpenSceneFileCommandId = "scene.open_file";
inline constexpr std::string_view SaveSceneFileCommandId = "scene.save_file";

void registerProjectCommands(CommandRegistry& registry);

} // namespace lunalite::tooling
