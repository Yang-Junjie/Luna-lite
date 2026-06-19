#include "../../LunaLite/animation/sprite_animation.h"
#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/project/project_manager.h"
#include "../../LunaLiteTooling/commands/asset_commands.h"
#include "../drag_drop.h"
#include "../editor_actions.h"
#include "animation_builder_panel.h"

#include <algorithm>
#include <imgui.h>
#include <optional>

namespace lunalite::editor {
namespace {
std::string assetDisplayName(asset::AssetHandle handle)
{
    const auto* metadata = asset::AssetManager::get().getMetadata(handle);
    if (metadata == nullptr) {
        return handle.isValid() ? "Missing asset " + handle.toString() : std::string{"None"};
    }

    if (!metadata->Name.empty()) {
        return metadata->Name;
    }
    return metadata->FilePath.filename().string();
}

std::filesystem::path parentDirectoryFor(asset::AssetHandle handle)
{
    const auto* metadata = asset::AssetManager::get().getMetadata(handle);
    if (metadata == nullptr) {
        return {};
    }
    return metadata->FilePath.parent_path();
}

std::optional<std::filesystem::path> assetsPath()
{
    const auto& info = project::ProjectManager::instance().getProjectInfo();
    if (!info) {
        return std::nullopt;
    }
    return info->assets_path.lexically_normal();
}

bool supportsFrameAsset(asset::AssetType type)
{
    return type == asset::AssetType::Sprite || type == asset::AssetType::Texture;
}

std::string clipModeLabel(asset::AssetHandle clip, bool dirty)
{
    if (!clip.isValid()) {
        return "Create Mode";
    }
    return "Edit Mode: " + assetDisplayName(clip) + (dirty ? " *" : "");
}
} // namespace

void AnimationBuilderPanel::onImGuiRender(bool* open)
{
    if (!ImGui::Begin("Animation Builder", open)) {
        ImGui::End();
        return;
    }

    const auto modeLabel = clipModeLabel(m_editing_clip, m_dirty);
    ImGui::TextUnformatted(modeLabel.c_str());
    ImGui::SameLine();
    if (ImGui::Button("New")) {
        resetBuilder();
    }

    ImGui::BeginDisabled(m_editing_clip.isValid());
    ImGui::InputText("Clip Name", m_clip_name.data(), m_clip_name.size());
    ImGui::InputText("Target Directory", m_target_directory.data(), m_target_directory.size());
    ImGui::EndDisabled();
    if (ImGui::DragFloat("FPS", &m_fps, 0.25f, 0.1f, 120.0f, "%.2f")) {
        markDirty();
    }
    if (ImGui::Checkbox("Loop", &m_loop)) {
        markDirty();
    }
    ImGui::Checkbox("Sort By Name", &m_sort_by_name);
    ImGui::Checkbox("Create Animator Controller", &m_create_controller);

    ImGui::Separator();
    ImGui::Text("Frames: %zu", m_frames.size());
    ImGui::SameLine();
    if (ImGui::Button("Sort")) {
        sortFramesByName();
        markDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_frames.clear();
        m_status.clear();
        markDirty();
    }

    ImGui::BeginChild("AnimationBuilderFrames", ImVec2(0.0f, 240.0f), true);
    for (size_t index = 0; index < m_frames.size(); ++index) {
        auto& frame = m_frames[index];
        ImGui::PushID(static_cast<int>(index));

        ImGui::Text("%03zu", index);
        ImGui::SameLine();
        ImGui::TextUnformatted(frameLabel(frame).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Up") && index > 0) {
            std::swap(m_frames[index], m_frames[index - 1]);
            markDirty();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Down") && index + 1 < m_frames.size()) {
            std::swap(m_frames[index], m_frames[index + 1]);
            markDirty();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            m_frames.erase(m_frames.begin() + static_cast<std::ptrdiff_t>(index));
            markDirty();
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (const auto payload = drag_drop::acceptAssetPayload()) {
        handleDroppedAsset(payload->handle, payload->type);
    }
    ImGui::EndChild();

    if (const auto payload = drag_drop::acceptAssetPayload()) {
        handleDroppedAsset(payload->handle, payload->type);
    }

    if (m_editing_clip.isValid() && ImGui::Button("Save")) {
        if (m_sort_by_name) {
            sortFramesByName();
        }
        saveAnimation();
    }
    if (m_editing_clip.isValid()) {
        ImGui::SameLine();
    }
    if (ImGui::Button(m_editing_clip.isValid() ? "Save As New" : "Create")) {
        if (m_sort_by_name) {
            sortFramesByName();
        }
        createAnimation();
    }

    if (!m_status.empty()) {
        const auto color = m_status_error ? ImVec4{1.0f, 0.25f, 0.2f, 1.0f} : ImVec4{0.35f, 0.85f, 0.35f, 1.0f};
        ImGui::TextColored(color, "%s", m_status.c_str());
    }

    ImGui::End();
}

void AnimationBuilderPanel::addFrame(asset::AssetHandle handle, asset::AssetType type)
{
    if (!handle.isValid() || !supportsFrameAsset(type)) {
        m_status = "Only Sprite or Texture assets can be added";
        m_status_error = true;
        return;
    }

    const auto* metadata = asset::AssetManager::get().getMetadata(handle);
    if (metadata == nullptr || metadata->Type != type) {
        m_status = "Dropped asset metadata was not found";
        m_status_error = true;
        return;
    }

    m_frames.push_back(FrameAsset{
        .handle = handle,
        .type = type,
    });

    if (m_frames.size() == 1) {
        const auto directory = parentDirectoryFor(handle);
        if (!directory.empty()) {
            setText(m_target_directory, directory.generic_string());
        }
    }

    markDirty();
    m_status.clear();
}

void AnimationBuilderPanel::handleDroppedAsset(asset::AssetHandle handle, asset::AssetType type)
{
    if (type == asset::AssetType::SpriteAnimationClip) {
        loadClip(handle);
        return;
    }

    addFrame(handle, type);
}

bool AnimationBuilderPanel::loadClip(asset::AssetHandle clip)
{
    const auto* metadata = asset::AssetManager::get().getMetadata(clip);
    if (metadata == nullptr || metadata->Type != asset::AssetType::SpriteAnimationClip) {
        m_status = "Dropped asset is not a sprite animation clip";
        m_status_error = true;
        return false;
    }

    const auto* animation = asset::AssetManager::get().getAsset<animation::SpriteAnimationClip>(clip);
    if (animation == nullptr) {
        m_status = "Sprite animation clip is not loaded";
        m_status_error = true;
        return false;
    }

    m_frames.clear();
    for (const auto& frame : animation->frames) {
        if (frame.sprite.isValid()) {
            m_frames.push_back(FrameAsset{
                .handle = frame.sprite,
                .type = asset::AssetType::Sprite,
            });
        }
    }

    m_editing_clip = clip;
    m_fps = animation->fps;
    m_loop = animation->loop;
    setText(m_clip_name, metadata->FilePath.stem().string());
    setText(m_target_directory, metadata->FilePath.parent_path().generic_string());
    m_dirty = false;
    m_status = "Loaded animation clip";
    m_status_error = false;
    return true;
}

void AnimationBuilderPanel::resetBuilder()
{
    m_editing_clip = asset::AssetHandle{0};
    m_frames.clear();
    m_fps = 12.0f;
    m_loop = true;
    m_create_controller = true;
    m_dirty = false;
    setText(m_clip_name, "NewSpriteAnimation");
    if (const auto path = assetsPath()) {
        setText(m_target_directory, path->generic_string());
    } else {
        setText(m_target_directory, "Assets");
    }
    m_status.clear();
    m_status_error = false;
}

void AnimationBuilderPanel::sortFramesByName()
{
    std::sort(m_frames.begin(), m_frames.end(), [](const FrameAsset& lhs, const FrameAsset& rhs) {
        return assetDisplayName(lhs.handle) < assetDisplayName(rhs.handle);
    });
}

bool AnimationBuilderPanel::createAnimation()
{
    if (m_frames.empty()) {
        m_status = "Add at least one frame";
        m_status_error = true;
        return false;
    }

    const std::string clipName = m_clip_name.data();
    if (clipName.empty()) {
        m_status = "Clip name is required";
        m_status_error = true;
        return false;
    }

    std::vector<asset::AssetHandle> spriteFrames;
    if (!collectSpriteFrames(spriteFrames)) {
        return false;
    }

    const auto targetDirectory = defaultTargetDirectory();
    tooling::CommandArgs clipArgs;
    clipArgs.set("target_directory", targetDirectory);
    clipArgs.set("name", clipName);
    clipArgs.set("fps", static_cast<double>(m_fps));
    clipArgs.set("loop", m_loop);
    clipArgs.set("frames", spriteFrames);
    const auto clipResult = actions::executeAssetCommand(tooling::CreateSpriteAnimationClipCommandId, clipArgs);
    if (!clipResult.success) {
        m_status = clipResult.message.empty() ? "Failed to create animation clip" : clipResult.message;
        m_status_error = true;
        return false;
    }

    const auto* clip = clipResult.get<asset::AssetHandle>("created_asset");
    if (clip == nullptr || !clip->isValid()) {
        m_status = "Animation clip command did not return an asset";
        m_status_error = true;
        return false;
    }

    if (m_create_controller) {
        tooling::CommandArgs controllerArgs;
        controllerArgs.set("target_directory", targetDirectory);
        controllerArgs.set("name", clipName + "Animator");
        controllerArgs.set("state_name", clipName);
        controllerArgs.set("clip", *clip);
        controllerArgs.set("loop", m_loop);
        const auto controllerResult =
            actions::executeAssetCommand(tooling::CreateSpriteAnimatorControllerCommandId, controllerArgs);
        if (!controllerResult.success) {
            m_status = controllerResult.message.empty()
                           ? "Clip created, controller creation failed"
                           : "Clip created, controller creation failed: " + controllerResult.message;
            m_status_error = true;
            return false;
        }
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        LUNA_CORE_ERROR("Failed to refresh project assets after animation build");
    }

    m_status = m_create_controller ? "Animation clip and controller created" : "Animation clip created";
    m_status_error = false;
    return true;
}

bool AnimationBuilderPanel::saveAnimation()
{
    if (!m_editing_clip.isValid()) {
        m_status = "No animation clip is loaded";
        m_status_error = true;
        return false;
    }
    if (m_frames.empty()) {
        m_status = "Add at least one frame";
        m_status_error = true;
        return false;
    }

    std::vector<asset::AssetHandle> spriteFrames;
    if (!collectSpriteFrames(spriteFrames)) {
        return false;
    }

    tooling::CommandArgs clipArgs;
    clipArgs.set("clip", m_editing_clip);
    clipArgs.set("fps", static_cast<double>(m_fps));
    clipArgs.set("loop", m_loop);
    clipArgs.set("frames", spriteFrames);
    const auto result = actions::executeAssetCommand(tooling::SaveSpriteAnimationClipCommandId, clipArgs);
    if (!result.success) {
        m_status = result.message.empty() ? "Failed to save animation clip" : result.message;
        m_status_error = true;
        return false;
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        LUNA_CORE_ERROR("Failed to refresh project assets after animation save");
    }

    m_frames.clear();
    for (const auto sprite : spriteFrames) {
        m_frames.push_back(FrameAsset{
            .handle = sprite,
            .type = asset::AssetType::Sprite,
        });
    }
    m_dirty = false;
    m_status = "Animation clip saved";
    m_status_error = false;
    return true;
}

bool AnimationBuilderPanel::collectSpriteFrames(std::vector<asset::AssetHandle>& sprite_frames)
{
    sprite_frames.clear();
    sprite_frames.reserve(m_frames.size());
    const auto targetDirectory = defaultTargetDirectory();

    for (const auto& frame : m_frames) {
        if (frame.type == asset::AssetType::Sprite) {
            sprite_frames.push_back(frame.handle);
            continue;
        }

        tooling::CommandArgs spriteArgs;
        spriteArgs.set("source_asset", frame.handle);
        spriteArgs.set("target_directory", targetDirectory);
        const auto spriteResult = actions::executeAssetCommand(tooling::CreateSpriteCommandId, spriteArgs);
        if (!spriteResult.success) {
            m_status = spriteResult.message.empty() ? "Failed to create sprite frame" : spriteResult.message;
            m_status_error = true;
            return false;
        }

        const auto* createdSprite = spriteResult.get<asset::AssetHandle>("created_asset");
        if (createdSprite == nullptr || !createdSprite->isValid()) {
            m_status = "Sprite frame command did not return an asset";
            m_status_error = true;
            return false;
        }
        sprite_frames.push_back(*createdSprite);
    }

    return true;
}

void AnimationBuilderPanel::markDirty()
{
    if (m_editing_clip.isValid()) {
        m_dirty = true;
    }
}

std::filesystem::path AnimationBuilderPanel::defaultTargetDirectory() const
{
    const std::filesystem::path configured = m_target_directory.data();
    if (!configured.empty()) {
        return configured.lexically_normal();
    }

    if (const auto path = assetsPath()) {
        return *path;
    }
    return "Assets";
}

std::string AnimationBuilderPanel::frameLabel(const FrameAsset& frame) const
{
    return assetDisplayName(frame.handle) + " (" + asset::assetTypeToString(frame.type) + ")";
}

void AnimationBuilderPanel::setText(std::array<char, 256>& target, const std::string& text)
{
    target.fill('\0');
    const auto count = std::min(text.size(), target.size() - 1);
    std::copy_n(text.data(), count, target.data());
}

} // namespace lunalite::editor
