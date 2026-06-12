#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/factory/asset_factory.h"
#include "../../LunaLite/asset/metadata/asset_metadata_serializer.h"
#include "../../LunaLite/asset/sprite.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/project/project_manager.h"
#include "../../LunaLite/renderer/interface/material.h"
#include "../../LunaLite/renderer/interface/texture.h"
#include "asset_commands.h"
#include "command_registry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>
#include <yaml-cpp/yaml.h>

namespace lunalite::tooling {
namespace {
std::optional<float> floatArg(const CommandArgs& args, std::string_view key)
{
    if (const auto value = args.get<double>(key)) {
        return static_cast<float>(*value);
    }
    if (const auto value = args.get<int64_t>(key)) {
        return static_cast<float>(*value);
    }
    if (const auto value = args.get<uint64_t>(key)) {
        return static_cast<float>(*value);
    }
    return std::nullopt;
}

std::optional<uint32_t> uint32Arg(const CommandArgs& args, std::string_view key)
{
    if (const auto value = args.get<uint64_t>(key)) {
        return static_cast<uint32_t>(*value);
    }
    if (const auto value = args.get<int64_t>(key)) {
        return static_cast<uint32_t>(std::max<int64_t>(*value, 0));
    }
    return std::nullopt;
}

std::optional<bool> boolArg(const CommandArgs& args, std::string_view key)
{
    return args.get<bool>(key);
}

asset::AssetHandle assetArg(const CommandArgs& args, std::string_view key, asset::AssetHandle fallback)
{
    const auto value = args.get<asset::AssetHandle>(key);
    return value ? *value : fallback;
}

renderer::interface::ShadingModel shadingModelFromValue(uint64_t value)
{
    return value == static_cast<uint64_t>(renderer::interface::ShadingModel::Unlit)
               ? renderer::interface::ShadingModel::Unlit
               : renderer::interface::ShadingModel::Lit;
}

const char* shadingModelToString(renderer::interface::ShadingModel shadingModel)
{
    switch (shadingModel) {
        case renderer::interface::ShadingModel::Unlit:
            return "Unlit";
        case renderer::interface::ShadingModel::Lit:
        default:
            return "Lit";
    }
}

asset::SpriteColorSpace spriteColorSpaceFromValue(uint64_t value)
{
    return value == static_cast<uint64_t>(asset::SpriteColorSpace::Linear) ? asset::SpriteColorSpace::Linear
                                                                           : asset::SpriteColorSpace::SRGB;
}

renderer::interface::TextureColorSpace textureColorSpaceFromValue(uint64_t value)
{
    return value == static_cast<uint64_t>(renderer::interface::TextureColorSpace::SRGB)
               ? renderer::interface::TextureColorSpace::SRGB
               : renderer::interface::TextureColorSpace::Linear;
}

renderer::interface::TextureFilter textureFilterFromValue(uint64_t value)
{
    return value == static_cast<uint64_t>(renderer::interface::TextureFilter::Nearest)
               ? renderer::interface::TextureFilter::Nearest
               : renderer::interface::TextureFilter::Linear;
}

renderer::interface::TextureMipFilter textureMipFilterFromValue(uint64_t value)
{
    switch (value) {
        case static_cast<uint64_t>(renderer::interface::TextureMipFilter::Nearest):
            return renderer::interface::TextureMipFilter::Nearest;
        case static_cast<uint64_t>(renderer::interface::TextureMipFilter::Linear):
            return renderer::interface::TextureMipFilter::Linear;
        case static_cast<uint64_t>(renderer::interface::TextureMipFilter::None):
        default:
            return renderer::interface::TextureMipFilter::None;
    }
}

renderer::interface::TextureAddressMode textureAddressModeFromValue(uint64_t value)
{
    switch (value) {
        case static_cast<uint64_t>(renderer::interface::TextureAddressMode::ClampToEdge):
            return renderer::interface::TextureAddressMode::ClampToEdge;
        case static_cast<uint64_t>(renderer::interface::TextureAddressMode::MirroredRepeat):
            return renderer::interface::TextureAddressMode::MirroredRepeat;
        case static_cast<uint64_t>(renderer::interface::TextureAddressMode::Repeat):
        default:
            return renderer::interface::TextureAddressMode::Repeat;
    }
}

const char* textureColorSpaceToString(renderer::interface::TextureColorSpace value)
{
    switch (value) {
        case renderer::interface::TextureColorSpace::SRGB:
            return "SRGB";
        case renderer::interface::TextureColorSpace::Linear:
        default:
            return "Linear";
    }
}

const char* textureFilterToString(renderer::interface::TextureFilter value)
{
    switch (value) {
        case renderer::interface::TextureFilter::Nearest:
            return "Nearest";
        case renderer::interface::TextureFilter::Linear:
        default:
            return "Linear";
    }
}

const char* textureMipFilterToString(renderer::interface::TextureMipFilter value)
{
    switch (value) {
        case renderer::interface::TextureMipFilter::Nearest:
            return "Nearest";
        case renderer::interface::TextureMipFilter::Linear:
            return "Linear";
        case renderer::interface::TextureMipFilter::None:
        default:
            return "None";
    }
}

const char* textureAddressModeToString(renderer::interface::TextureAddressMode value)
{
    switch (value) {
        case renderer::interface::TextureAddressMode::ClampToEdge:
            return "ClampToEdge";
        case renderer::interface::TextureAddressMode::MirroredRepeat:
            return "MirroredRepeat";
        case renderer::interface::TextureAddressMode::Repeat:
        default:
            return "Repeat";
    }
}

void applyMaterialParameterArgs(renderer::interface::MaterialParameters& parameters, const CommandArgs& args)
{
    if (const auto shadingModel = args.get<uint64_t>("shading_model")) {
        parameters.shading_model = shadingModelFromValue(*shadingModel);
    }

    if (const auto value = floatArg(args, "albedo_r")) {
        parameters.albedo.r = *value;
    }
    if (const auto value = floatArg(args, "albedo_g")) {
        parameters.albedo.g = *value;
    }
    if (const auto value = floatArg(args, "albedo_b")) {
        parameters.albedo.b = *value;
    }
    if (const auto value = floatArg(args, "albedo_a")) {
        parameters.albedo.a = *value;
    }
    if (const auto value = floatArg(args, "metallic")) {
        parameters.metallic = *value;
    }
    if (const auto value = floatArg(args, "roughness")) {
        parameters.roughness = *value;
    }
    if (const auto value = floatArg(args, "emission_r")) {
        parameters.emission.r = *value;
    }
    if (const auto value = floatArg(args, "emission_g")) {
        parameters.emission.g = *value;
    }
    if (const auto value = floatArg(args, "emission_b")) {
        parameters.emission.b = *value;
    }
    if (const auto value = floatArg(args, "emission_strength")) {
        parameters.emission_strength = *value;
    }
    if (const auto value = floatArg(args, "normal_scale")) {
        parameters.normal_scale = *value;
    }
    if (const auto value = floatArg(args, "occlusion_strength")) {
        parameters.occlusion_strength = *value;
    }

    parameters.albedo_texture = assetArg(args, "albedo_texture", parameters.albedo_texture);
    parameters.normal_texture = assetArg(args, "normal_texture", parameters.normal_texture);
    parameters.metallic_roughness_texture =
        assetArg(args, "metallic_roughness_texture", parameters.metallic_roughness_texture);
    parameters.occlusion_texture = assetArg(args, "occlusion_texture", parameters.occlusion_texture);
    parameters.emission_texture = assetArg(args, "emission_texture", parameters.emission_texture);

    parameters.metallic = std::clamp(parameters.metallic, 0.0f, 1.0f);
    parameters.roughness = std::clamp(parameters.roughness, 0.0f, 1.0f);
    parameters.emission_strength = std::max(parameters.emission_strength, 0.0f);
    parameters.normal_scale = std::max(parameters.normal_scale, 0.0f);
    parameters.occlusion_strength = std::clamp(parameters.occlusion_strength, 0.0f, 1.0f);
}

void applySpriteParameterArgs(asset::Sprite& sprite, const CommandArgs& args)
{
    sprite.texture = assetArg(args, "texture", sprite.texture);
    if (const auto value = uint32Arg(args, "source_x")) {
        sprite.source_rect.x = *value;
    }
    if (const auto value = uint32Arg(args, "source_y")) {
        sprite.source_rect.y = *value;
    }
    if (const auto value = uint32Arg(args, "source_width")) {
        sprite.source_rect.width = *value;
    }
    if (const auto value = uint32Arg(args, "source_height")) {
        sprite.source_rect.height = *value;
    }
    if (const auto value = floatArg(args, "pivot_x")) {
        sprite.pivot.x = *value;
    }
    if (const auto value = floatArg(args, "pivot_y")) {
        sprite.pivot.y = *value;
    }
    if (const auto value = floatArg(args, "pixels_per_unit")) {
        sprite.pixels_per_unit = std::max(*value, 0.0001f);
    }
    if (const auto value = boolArg(args, "flip_y")) {
        sprite.flip_y = *value;
    }
    if (const auto value = args.get<uint64_t>("color_space")) {
        sprite.color_space = spriteColorSpaceFromValue(*value);
    }
}

void applyTextureImportSettingArgs(renderer::interface::TextureImportSettings& settings, const CommandArgs& args)
{
    if (const auto value = boolArg(args, "generate_mipmaps")) {
        settings.generate_mipmaps = *value;
    }
    if (const auto value = args.get<uint64_t>("color_space")) {
        settings.color_space = textureColorSpaceFromValue(*value);
    }
    if (const auto value = args.get<uint64_t>("min_filter")) {
        settings.sampler.min_filter = textureFilterFromValue(*value);
    }
    if (const auto value = args.get<uint64_t>("mag_filter")) {
        settings.sampler.mag_filter = textureFilterFromValue(*value);
    }
    if (const auto value = args.get<uint64_t>("mip_filter")) {
        settings.sampler.mip_filter = textureMipFilterFromValue(*value);
    }
    if (const auto value = args.get<uint64_t>("address_u")) {
        settings.sampler.address_u = textureAddressModeFromValue(*value);
    }
    if (const auto value = args.get<uint64_t>("address_v")) {
        settings.sampler.address_v = textureAddressModeFromValue(*value);
    }
    if (const auto value = args.get<uint64_t>("address_w")) {
        settings.sampler.address_w = textureAddressModeFromValue(*value);
    }
}

CommandResult resolveLoadedMaterial(ToolContext& context,
                                    const CommandArgs& args,
                                    renderer::interface::Material*& material,
                                    const asset::AssetMetadata*& metadata)
{
    const auto handle = args.get<asset::AssetHandle>("material");
    if (!handle || !handle->isValid()) {
        return CommandResult::fail("asset material command requires a valid material");
    }

    metadata = context.assetManager().getMetadata(*handle);
    if (metadata == nullptr || metadata->Type != asset::AssetType::Material) {
        return CommandResult::fail("asset material command requires a material asset");
    }

    material = context.assetManager().getAsset<renderer::interface::Material>(*handle);
    if (material == nullptr || material->parameters == nullptr) {
        return CommandResult::fail("Material asset is not loaded");
    }

    return CommandResult::ok();
}

CommandResult resolveLoadedSprite(ToolContext& context,
                                  const CommandArgs& args,
                                  asset::Sprite*& sprite,
                                  const asset::AssetMetadata*& metadata)
{
    const auto handle = args.get<asset::AssetHandle>("sprite");
    if (!handle || !handle->isValid()) {
        return CommandResult::fail("asset sprite command requires a valid sprite");
    }

    metadata = context.assetManager().getMetadata(*handle);
    if (metadata == nullptr || metadata->Type != asset::AssetType::Sprite) {
        return CommandResult::fail("asset sprite command requires a sprite asset");
    }

    sprite = context.assetManager().getAsset<asset::Sprite>(*handle);
    if (sprite == nullptr) {
        return CommandResult::fail("Sprite asset is not loaded");
    }

    return CommandResult::ok();
}

CommandResult resolveLoadedTexture(ToolContext& context,
                                   const CommandArgs& args,
                                   renderer::interface::Texture*& texture,
                                   const asset::AssetMetadata*& metadata)
{
    const auto handle = args.get<asset::AssetHandle>("texture");
    if (!handle || !handle->isValid()) {
        return CommandResult::fail("asset texture command requires a valid texture");
    }

    metadata = context.assetManager().getMetadata(*handle);
    if (metadata == nullptr || metadata->Type != asset::AssetType::Texture) {
        return CommandResult::fail("asset texture command requires a texture asset");
    }

    texture = context.assetManager().getAsset<renderer::interface::Texture>(*handle);
    if (texture == nullptr) {
        return CommandResult::fail("Texture asset is not loaded");
    }

    return CommandResult::ok();
}

bool writeMaterialFile(const asset::AssetMetadata& metadata, const renderer::interface::MaterialParameters& parameters)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        LUNA_CORE_ERROR("Failed to create material directory '{}': {}", path.parent_path().string(), error.message());
        return false;
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open material file for writing: '{}'", path.string());
        return false;
    }

    out << "Material:\n";
    out << "  ShadingModel: " << shadingModelToString(parameters.shading_model) << "\n";
    out << "  Albedo: [" << parameters.albedo.r << ", " << parameters.albedo.g << ", " << parameters.albedo.b << ", "
        << parameters.albedo.a << "]\n";
    out << "  Metallic: " << parameters.metallic << "\n";
    out << "  Roughness: " << parameters.roughness << "\n";
    out << "  Emission: [" << parameters.emission.r << ", " << parameters.emission.g << ", " << parameters.emission.b
        << "]\n";
    out << "  EmissionStrength: " << parameters.emission_strength << "\n";
    out << "  NormalScale: " << parameters.normal_scale << "\n";
    out << "  OcclusionStrength: " << parameters.occlusion_strength << "\n";
    out << "  Textures:\n";
    out << "    Albedo: " << static_cast<uint64_t>(parameters.albedo_texture) << "\n";
    out << "    Normal: " << static_cast<uint64_t>(parameters.normal_texture) << "\n";
    out << "    MetallicRoughness: " << static_cast<uint64_t>(parameters.metallic_roughness_texture) << "\n";
    out << "    Occlusion: " << static_cast<uint64_t>(parameters.occlusion_texture) << "\n";
    out << "    Emission: " << static_cast<uint64_t>(parameters.emission_texture) << "\n";

    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write material file: '{}'", path.string());
        return false;
    }

    return true;
}

bool writeSpriteFile(const asset::AssetMetadata& metadata, const asset::Sprite& sprite)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        LUNA_CORE_ERROR("Failed to create sprite directory '{}': {}", path.parent_path().string(), error.message());
        return false;
    }

    YAML::Node root;
    root["Sprite"]["Texture"] = static_cast<uint64_t>(sprite.texture);
    root["Sprite"]["SourceRect"]["X"] = sprite.source_rect.x;
    root["Sprite"]["SourceRect"]["Y"] = sprite.source_rect.y;
    root["Sprite"]["SourceRect"]["Width"] = sprite.source_rect.width;
    root["Sprite"]["SourceRect"]["Height"] = sprite.source_rect.height;
    root["Sprite"]["Pivot"]["X"] = sprite.pivot.x;
    root["Sprite"]["Pivot"]["Y"] = sprite.pivot.y;
    root["Sprite"]["PixelsPerUnit"] = sprite.pixels_per_unit;
    root["Sprite"]["FlipY"] = sprite.flip_y;
    root["Sprite"]["ColorSpace"] = asset::spriteColorSpaceToString(sprite.color_space);

    std::ofstream out(path);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open sprite file for writing: '{}'", path.string());
        return false;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write sprite file: '{}'", path.string());
        return false;
    }

    return true;
}

YAML::Node textureImportSettingsToConfig(const renderer::interface::TextureImportSettings& settings,
                                         const YAML::Node& existingConfig)
{
    YAML::Node config = existingConfig ? YAML::Clone(existingConfig) : YAML::Node{};
    config["GenerateMipmaps"] = settings.generate_mipmaps;
    config["ColorSpace"] = textureColorSpaceToString(settings.color_space);
    config["Sampler"]["MinFilter"] = textureFilterToString(settings.sampler.min_filter);
    config["Sampler"]["MagFilter"] = textureFilterToString(settings.sampler.mag_filter);
    config["Sampler"]["MipFilter"] = textureMipFilterToString(settings.sampler.mip_filter);
    config["Sampler"]["AddressU"] = textureAddressModeToString(settings.sampler.address_u);
    config["Sampler"]["AddressV"] = textureAddressModeToString(settings.sampler.address_v);
    config["Sampler"]["AddressW"] = textureAddressModeToString(settings.sampler.address_w);
    return config;
}

bool writeTextureMetadata(const asset::AssetMetadata& metadata,
                          const renderer::interface::TextureImportSettings& settings)
{
    auto updatedMetadata = metadata;
    updatedMetadata.SpecializedConfig = textureImportSettingsToConfig(settings, metadata.SpecializedConfig);
    return asset::AssetMetadataSerializer{}.write(updatedMetadata);
}
} // namespace

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
        return CommandResult::fail(factoryResult.error.empty() ? "Failed to create sprite asset" : factoryResult.error);
    }

    auto result = CommandResult::ok("Sprite asset created");
    result.set("created_asset", factoryResult.handle);
    result.set("created_path", factoryResult.path);
    return result;
}

std::string_view SetMaterialParametersCommand::id() const
{
    return SetMaterialParametersCommandId;
}

std::string_view SetMaterialParametersCommand::label() const
{
    return "Set Material Parameters";
}

std::string_view SetMaterialParametersCommand::category() const
{
    return "Asset";
}

CommandResult SetMaterialParametersCommand::execute(ToolContext& context, const CommandArgs& args)
{
    renderer::interface::Material* material = nullptr;
    const asset::AssetMetadata* metadata = nullptr;
    auto resolved = resolveLoadedMaterial(context, args, material, metadata);
    if (!resolved.success) {
        return resolved;
    }

    applyMaterialParameterArgs(*material->parameters, args);

    auto result = CommandResult::ok("Material parameters changed");
    result.set("material", material->handle);
    return result;
}

std::string_view SaveMaterialCommand::id() const
{
    return SaveMaterialCommandId;
}

std::string_view SaveMaterialCommand::label() const
{
    return "Save Material";
}

std::string_view SaveMaterialCommand::category() const
{
    return "Asset";
}

CommandResult SaveMaterialCommand::execute(ToolContext& context, const CommandArgs& args)
{
    renderer::interface::Material* material = nullptr;
    const asset::AssetMetadata* metadata = nullptr;
    auto resolved = resolveLoadedMaterial(context, args, material, metadata);
    if (!resolved.success) {
        return resolved;
    }
    if (metadata->MemoryOnly) {
        return CommandResult::fail("Cannot save a memory-only material asset");
    }

    if (!writeMaterialFile(*metadata, *material->parameters)) {
        return CommandResult::fail("Failed to save material");
    }

    auto result = CommandResult::ok("Material saved");
    result.set("material", material->handle);
    result.set("saved_path", metadata->FilePath);
    return result;
}

std::string_view SetSpriteParametersCommand::id() const
{
    return SetSpriteParametersCommandId;
}

std::string_view SetSpriteParametersCommand::label() const
{
    return "Set Sprite Parameters";
}

std::string_view SetSpriteParametersCommand::category() const
{
    return "Asset";
}

CommandResult SetSpriteParametersCommand::execute(ToolContext& context, const CommandArgs& args)
{
    asset::Sprite* sprite = nullptr;
    const asset::AssetMetadata* metadata = nullptr;
    auto resolved = resolveLoadedSprite(context, args, sprite, metadata);
    if (!resolved.success) {
        return resolved;
    }

    applySpriteParameterArgs(*sprite, args);

    auto result = CommandResult::ok("Sprite parameters changed");
    result.set("sprite", sprite->handle);
    return result;
}

std::string_view SaveSpriteCommand::id() const
{
    return SaveSpriteCommandId;
}

std::string_view SaveSpriteCommand::label() const
{
    return "Save Sprite";
}

std::string_view SaveSpriteCommand::category() const
{
    return "Asset";
}

CommandResult SaveSpriteCommand::execute(ToolContext& context, const CommandArgs& args)
{
    asset::Sprite* sprite = nullptr;
    const asset::AssetMetadata* metadata = nullptr;
    auto resolved = resolveLoadedSprite(context, args, sprite, metadata);
    if (!resolved.success) {
        return resolved;
    }
    if (metadata->MemoryOnly) {
        return CommandResult::fail("Cannot save a memory-only sprite asset");
    }

    if (!writeSpriteFile(*metadata, *sprite)) {
        return CommandResult::fail("Failed to save sprite");
    }

    auto result = CommandResult::ok("Sprite saved");
    result.set("sprite", sprite->handle);
    result.set("saved_path", metadata->FilePath);
    return result;
}

std::string_view SetTextureImportSettingsCommand::id() const
{
    return SetTextureImportSettingsCommandId;
}

std::string_view SetTextureImportSettingsCommand::label() const
{
    return "Set Texture Import Settings";
}

std::string_view SetTextureImportSettingsCommand::category() const
{
    return "Asset";
}

CommandResult SetTextureImportSettingsCommand::execute(ToolContext& context, const CommandArgs& args)
{
    renderer::interface::Texture* texture = nullptr;
    const asset::AssetMetadata* metadata = nullptr;
    auto resolved = resolveLoadedTexture(context, args, texture, metadata);
    if (!resolved.success) {
        return resolved;
    }

    auto settings = texture->getImportSettings();
    applyTextureImportSettingArgs(settings, args);
    texture->setImportSettings(settings);

    auto result = CommandResult::ok("Texture import settings changed");
    result.set("texture", texture->handle);
    return result;
}

std::string_view SaveTextureImportSettingsCommand::id() const
{
    return SaveTextureImportSettingsCommandId;
}

std::string_view SaveTextureImportSettingsCommand::label() const
{
    return "Save Texture Import Settings";
}

std::string_view SaveTextureImportSettingsCommand::category() const
{
    return "Asset";
}

CommandResult SaveTextureImportSettingsCommand::execute(ToolContext& context, const CommandArgs& args)
{
    renderer::interface::Texture* texture = nullptr;
    const asset::AssetMetadata* metadata = nullptr;
    auto resolved = resolveLoadedTexture(context, args, texture, metadata);
    if (!resolved.success) {
        return resolved;
    }
    if (metadata->MemoryOnly) {
        return CommandResult::fail("Cannot save a memory-only texture asset");
    }

    if (!writeTextureMetadata(*metadata, texture->getImportSettings())) {
        return CommandResult::fail("Failed to save texture import settings");
    }

    auto result = CommandResult::ok("Texture import settings saved");
    result.set("texture", texture->handle);
    result.set("saved_path", metadata->FilePath);
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
    registry.registerCommand(std::make_unique<SetMaterialParametersCommand>());
    registry.registerCommand(std::make_unique<SaveMaterialCommand>());
    registry.registerCommand(std::make_unique<SetSpriteParametersCommand>());
    registry.registerCommand(std::make_unique<SaveSpriteCommand>());
    registry.registerCommand(std::make_unique<SetTextureImportSettingsCommand>());
    registry.registerCommand(std::make_unique<SaveTextureImportSettingsCommand>());
}

} // namespace lunalite::tooling
