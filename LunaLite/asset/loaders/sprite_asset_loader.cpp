#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "sprite_asset_loader.h"

#include <algorithm>
#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {

glm::vec2 readPivot(const YAML::Node& node, const glm::vec2& fallback)
{
    if (!node) {
        return fallback;
    }
    if (node.IsSequence() && node.size() == 2) {
        return glm::vec2{node[0].as<float>(fallback.x), node[1].as<float>(fallback.y)};
    }
    return glm::vec2{node["X"].as<float>(fallback.x), node["Y"].as<float>(fallback.y)};
}

SpriteSourceRect readSourceRect(const YAML::Node& node)
{
    if (!node) {
        return {};
    }

    return SpriteSourceRect{
        .x = node["X"].as<uint32_t>(0),
        .y = node["Y"].as<uint32_t>(0),
        .width = node["Width"].as<uint32_t>(0),
        .height = node["Height"].as<uint32_t>(0),
    };
}

} // namespace

std::shared_ptr<Sprite> SpriteAssetLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    try {
        const auto root = YAML::LoadFile(path.string());
        const auto spriteNode = root["Sprite"] ? root["Sprite"] : root;

        auto sprite = std::make_shared<Sprite>();
        sprite->handle = metadata.Handle;
        sprite->texture = AssetHandle{spriteNode["Texture"].as<uint64_t>(0)};
        sprite->source_rect = readSourceRect(spriteNode["SourceRect"]);
        sprite->pivot = readPivot(spriteNode["Pivot"], sprite->pivot);
        sprite->pixels_per_unit = std::max(spriteNode["PixelsPerUnit"].as<float>(100.0f), 0.0001f);
        sprite->flip_y = spriteNode["FlipY"].as<bool>(true);
        sprite->color_space = spriteColorSpaceFromString(spriteNode["ColorSpace"].as<std::string>("SRGB"));

        if (!sprite->texture.isValid()) {
            LUNA_CORE_ERROR("Failed to load sprite '{}': Texture handle is invalid", path.string());
            return nullptr;
        }

        LUNA_CORE_DEBUG("Loaded sprite '{}' (texture: {}, source: {}x{}, ppu: {}, handle {})",
                        path.string(),
                        sprite->texture.toString(),
                        sprite->source_rect.width,
                        sprite->source_rect.height,
                        sprite->pixels_per_unit,
                        metadata.Handle.toString());
        return sprite;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load sprite '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load sprite '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}

} // namespace lunalite::asset
