#pragma once

#include "../../LunaLite/asset/asset.h"
#include "../../LunaLite/asset/asset_types.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace lunalite::editor {

class AnimationBuilderPanel {
public:
    void onImGuiRender(bool* open = nullptr);

private:
    struct FrameAsset {
        asset::AssetHandle handle{0};
        asset::AssetType type{asset::AssetType::None};
    };

    void addFrame(asset::AssetHandle handle, asset::AssetType type);
    void handleDroppedAsset(asset::AssetHandle handle, asset::AssetType type);
    bool loadClip(asset::AssetHandle clip);
    void resetBuilder();
    void sortFramesByName();
    bool createAnimation();
    bool saveAnimation();
    bool collectSpriteFrames(std::vector<asset::AssetHandle>& sprite_frames);
    void markDirty();
    std::filesystem::path defaultTargetDirectory() const;
    std::string frameLabel(const FrameAsset& frame) const;
    void setText(std::array<char, 256>& target, const std::string& text);

    std::array<char, 256> m_clip_name{"NewSpriteAnimation"};
    std::array<char, 256> m_target_directory{"Assets"};
    std::vector<FrameAsset> m_frames;
    asset::AssetHandle m_editing_clip{0};
    float m_fps{12.0f};
    bool m_loop{true};
    bool m_sort_by_name{true};
    bool m_create_controller{true};
    bool m_dirty{false};
    std::string m_status;
    bool m_status_error{false};
};

} // namespace lunalite::editor
