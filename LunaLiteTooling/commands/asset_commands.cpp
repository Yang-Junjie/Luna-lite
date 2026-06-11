#include "../../LunaLite/asset/factory/asset_factory.h"
#include "asset_commands.h"
#include "command_registry.h"

#include <filesystem>
#include <memory>

namespace lunalite::tooling {
std::string_view CreateSpriteCommand::id() const
{
    return CreateSpriteCommandId;
}

std::string_view CreateSpriteCommand::label() const
{
    return "Create Sprite";
}

std::string_view CreateSpriteCommand::category() const
{
    return "Asset";
}

CommandResult CreateSpriteCommand::execute(ToolContext& context, const CommandArgs& args)
{
    const auto sourceAsset = args.get<asset::AssetHandle>("source_asset");
    if (!sourceAsset || !sourceAsset->isValid()) {
        return CommandResult::fail("asset.create_sprite requires a valid source_asset");
    }

    const auto targetDirectory = args.get<std::filesystem::path>("target_directory");
    if (!targetDirectory) {
        return CommandResult::fail("asset.create_sprite requires target_directory");
    }

    const auto* sourceMetadata = context.assetManager().getMetadata(*sourceAsset);
    if (sourceMetadata == nullptr) {
        return CommandResult::fail("Source asset metadata was not found");
    }

    const asset::AssetFactoryContext factoryContext{
        .source = sourceMetadata,
        .target_directory = *targetDirectory,
    };
    const auto factoryResult = context.assetFactoryManager().create(SpriteFromTextureFactoryId, factoryContext);
    if (!factoryResult.handle.isValid()) {
        return CommandResult::fail(factoryResult.error.empty() ? "Failed to create sprite asset"
                                                               : factoryResult.error);
    }

    auto result = CommandResult::ok("Sprite asset created");
    result.set("created_asset", factoryResult.handle);
    result.set("created_path", factoryResult.path);
    return result;
}

std::optional<std::string_view> commandIdForAssetFactory(std::string_view factory_id)
{
    if (factory_id == SpriteFromTextureFactoryId) {
        return CreateSpriteCommandId;
    }

    return std::nullopt;
}

void registerAssetCommands(CommandRegistry& registry)
{
    registry.registerCommand(std::make_unique<CreateSpriteCommand>());
}

} // namespace lunalite::tooling
