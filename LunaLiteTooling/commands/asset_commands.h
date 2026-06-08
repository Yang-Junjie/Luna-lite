#pragma once

#include <optional>
#include <string_view>

namespace lunalite::tooling {

class CommandRegistry;

inline constexpr std::string_view CreateSpriteCommandId = "asset.create_sprite";
inline constexpr std::string_view SpriteFromTextureFactoryId = "SpriteFromTexture";

std::optional<std::string_view> commandIdForAssetFactory(std::string_view factory_id);
void registerAssetCommands(CommandRegistry& registry);

} // namespace lunalite::tooling
