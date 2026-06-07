#include "../../project/project_manager.h"
#include "../../renderer/interface/texture.h"
#include "../asset_manager.h"
#include "../sprite.h"
#include "sprite_asset_factory.h"

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <string>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {
std::filesystem::path resolveTargetDirectory(const std::filesystem::path& directory)
{
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot) {
        return {};
    }

    return (directory.is_absolute() ? directory : *projectRoot / directory).lexically_normal();
}

std::filesystem::path uniqueSpriteAssetPath(const std::filesystem::path& texturePath)
{
    auto candidate = texturePath;
    candidate.replace_extension(".lunasprite");
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }

    const auto directory = candidate.parent_path();
    const auto stem = candidate.stem().string();
    for (uint32_t index = 1; index < 1'000; ++index) {
        auto numbered = directory / (stem + "_" + std::to_string(index));
        numbered.replace_extension(".lunasprite");
        if (!std::filesystem::exists(numbered)) {
            return numbered;
        }
    }

    return {};
}

bool writeSpriteAsset(const std::filesystem::path& path, const Sprite& sprite)
{
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
    root["Sprite"]["ColorSpace"] = spriteColorSpaceToString(sprite.color_space);

    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }
    out << root;
    return out.good();
}
} // namespace

std::string_view SpriteAssetFactory::id() const
{
    return "SpriteFromTexture";
}

std::string_view SpriteAssetFactory::label() const
{
    return "Create Sprite";
}

AssetType SpriteAssetFactory::outputType() const
{
    return AssetType::Sprite;
}

bool SpriteAssetFactory::canCreate(const AssetFactoryContext& context) const
{
    return context.source != nullptr && context.source->Type == AssetType::Texture && context.source->Handle.isValid();
}

AssetFactoryResult SpriteAssetFactory::create(const AssetFactoryContext& context)
{
    if (!canCreate(context)) {
        return AssetFactoryResult{.error = "Sprite assets can only be created from Texture assets"};
    }

    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot) {
        return AssetFactoryResult{.error = "No project is loaded"};
    }

    const auto* texture = AssetManager::get().getAsset<renderer::interface::Texture>(context.source->Handle);
    if (texture == nullptr || texture->getWidth() == 0 || texture->getHeight() == 0) {
        return AssetFactoryResult{.error = "Source texture is not loaded"};
    }

    const auto targetDirectory = resolveTargetDirectory(context.target_directory);
    if (targetDirectory.empty()) {
        return AssetFactoryResult{.error = "Failed to resolve target directory"};
    }

    const auto texturePath = (*projectRoot / context.source->FilePath).lexically_normal();
    auto spritePath = uniqueSpriteAssetPath(targetDirectory / texturePath.filename());
    if (spritePath.empty()) {
        return AssetFactoryResult{.error = "No available sprite asset file name"};
    }

    Sprite sprite;
    sprite.texture = context.source->Handle;
    sprite.source_rect = SpriteSourceRect{
        .x = 0,
        .y = 0,
        .width = texture->getWidth(),
        .height = texture->getHeight(),
    };
    sprite.pivot = {0.5f, 0.5f};
    sprite.pixels_per_unit = 100.0f;
    sprite.flip_y = true;
    sprite.color_space = SpriteColorSpace::SRGB;

    if (!writeSpriteAsset(spritePath, sprite)) {
        return AssetFactoryResult{.path = spritePath, .error = "Failed to write sprite asset file"};
    }

    const auto handle = AssetManager::get().importAndLoadAsset(spritePath);
    if (!handle || !handle->isValid()) {
        return AssetFactoryResult{.path = spritePath, .error = "Failed to import and load sprite asset"};
    }

    return AssetFactoryResult{
        .handle = *handle,
        .path = spritePath,
    };
}

} // namespace lunalite::asset
