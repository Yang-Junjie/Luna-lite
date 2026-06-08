#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/asset/sprite.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLiteTooling/commands/asset_commands.h"
#include "../LunaLiteTooling/commands/command_registry.h"
#include "../LunaLiteTooling/context/tool_context.h"
#include "../third_party/stb/stb_image_write.h"

#include <array>
#include <filesystem>
#include <iostream>

namespace {
bool writeTestPngTexture(const std::filesystem::path& path)
{
    constexpr std::array<unsigned char, 16> pixels{
        255,
        0,
        0,
        255,
        0,
        255,
        0,
        255,
        0,
        0,
        255,
        255,
        255,
        255,
        255,
        255,
    };
    return stbi_write_png(path.string().c_str(), 2, 2, 4, pixels.data(), 2 * 4) != 0;
}
} // namespace

int main()
{
    using namespace lunalite;

    const auto projectRoot = std::filesystem::current_path() / "build" / "tooling_command_test_project";
    std::error_code error;
    std::filesystem::remove_all(projectRoot, error);

    project::ProjectInfo info;
    info.name = "ToolingCommandTestProject";
    info.assets_path = "Assets";
    if (!project::ProjectManager::instance().createProject(projectRoot, info)) {
        std::cerr << "Failed to create test project.\n";
        return 1;
    }

    const auto texturePath = projectRoot / info.assets_path / "texture.png";
    if (!writeTestPngTexture(texturePath)) {
        std::cerr << "Failed to write test texture.\n";
        return 1;
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        std::cerr << "Failed to load test project assets.\n";
        return 1;
    }

    const auto textureHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/texture.png");
    if (!textureHandle.isValid()) {
        std::cerr << "Failed to resolve texture handle.\n";
        return 1;
    }

    tooling::ToolContext context;
    tooling::CommandArgs args;
    args.set("source_asset", textureHandle);
    args.set("target_directory", std::filesystem::path{"Assets"});

    const auto result = tooling::CommandRegistry::get().execute(tooling::CreateSpriteCommandId, context, args);
    if (!result.success) {
        std::cerr << "asset.create_sprite failed: " << result.message << "\n";
        return 1;
    }

    const auto* createdAsset = result.get<asset::AssetHandle>("created_asset");
    const auto* createdPath = result.get<std::filesystem::path>("created_path");
    if (createdAsset == nullptr || !createdAsset->isValid() || createdPath == nullptr ||
        !std::filesystem::exists(*createdPath)) {
        std::cerr << "asset.create_sprite did not report the created sprite asset.\n";
        return 1;
    }

    const auto* sprite = asset::AssetManager::get().getAsset<asset::Sprite>(*createdAsset);
    if (sprite == nullptr || sprite->texture != textureHandle || sprite->source_rect.width != 2 ||
        sprite->source_rect.height != 2 || sprite->pivot.x != 0.5f || sprite->pivot.y != 0.5f ||
        sprite->pixels_per_unit != 100.0f || !sprite->flip_y || sprite->color_space != asset::SpriteColorSpace::SRGB) {
        std::cerr << "asset.create_sprite created an unexpected sprite asset.\n";
        return 1;
    }

    std::cout << "Tooling command asset.create_sprite created and loaded a sprite asset.\n";
    return 0;
}
