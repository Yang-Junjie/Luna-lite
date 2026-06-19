#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "sprite_animation_clip_loader.h"

#include <algorithm>
#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace lunalite::asset {
namespace {
animation::SpriteAnimationFrame readFrame(const YAML::Node& node, float defaultDuration)
{
    if (node.IsScalar()) {
        return animation::SpriteAnimationFrame{
            .sprite = AssetHandle{node.as<uint64_t>(0)},
            .duration = defaultDuration,
        };
    }

    return animation::SpriteAnimationFrame{
        .sprite = AssetHandle{node["Sprite"].as<uint64_t>(0)},
        .duration = std::max(node["Duration"].as<float>(defaultDuration), 0.0001f),
    };
}
} // namespace

std::shared_ptr<animation::SpriteAnimationClip> SpriteAnimationClipLoader::load(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    try {
        const auto root = YAML::LoadFile(path.string());
        const auto clipNode = root["SpriteAnimation"] ? root["SpriteAnimation"] : root;

        auto clip = std::make_shared<animation::SpriteAnimationClip>();
        clip->handle = metadata.Handle;
        clip->fps = std::max(clipNode["FPS"].as<float>(12.0f), 0.0001f);
        clip->loop = clipNode["Loop"].as<bool>(true);

        const float defaultDuration = 1.0f / clip->fps;
        if (const auto framesNode = clipNode["Frames"]; framesNode && framesNode.IsSequence()) {
            clip->frames.reserve(framesNode.size());
            for (const auto& frameNode : framesNode) {
                auto frame = readFrame(frameNode, defaultDuration);
                if (frame.sprite.isValid()) {
                    clip->frames.push_back(frame);
                }
            }
        }

        LUNA_CORE_DEBUG("Loaded sprite animation '{}' (frames: {}, fps: {}, loop: {}, handle {})",
                        path.string(),
                        clip->frames.size(),
                        clip->fps,
                        clip->loop,
                        metadata.Handle.toString());
        return clip;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to load sprite animation '{}': {}", path.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to load sprite animation '{}': {}", path.string(), exception.what());
    }

    return nullptr;
}

} // namespace lunalite::asset
