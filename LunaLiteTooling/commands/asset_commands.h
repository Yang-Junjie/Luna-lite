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

inline constexpr std::string_view CreateSpriteCommandId = "asset.create_sprite";
inline constexpr std::string_view SpriteFromTextureFactoryId = "SpriteFromTexture";

std::optional<std::string_view> commandIdForAssetFactory(std::string_view factory_id);
void registerAssetCommands(CommandRegistry& registry);

} // namespace lunalite::tooling
