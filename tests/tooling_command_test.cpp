#include "../LunaLite/animation/sprite_animation.h"
#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/asset/sprite.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/renderer/interface/material.h"
#include "../LunaLite/renderer/interface/texture.h"
#include "../LunaLiteTooling/commands/asset_commands.h"
#include "../LunaLiteTooling/commands/command_registry.h"
#include "../LunaLiteTooling/context/tool_context.h"
#include "../third_party/stb/stb_image_write.h"

#include <cmath>

#include <array>
#include <filesystem>
#include <fstream>
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

bool nearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= 0.001f;
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

    const auto materialPath = projectRoot / info.assets_path / "Editable.lunamat";
    {
        std::ofstream material(materialPath);
        material << "Material:\n";
        material << "  ShadingModel: Lit\n";
        material << "  Albedo: [0.8, 0.7, 0.6, 1.0]\n";
        material << "  Metallic: 0.0\n";
        material << "  Roughness: 0.5\n";
        material << "  Emission: [0.0, 0.0, 0.0]\n";
        material << "  EmissionStrength: 0.0\n";
        material << "  NormalScale: 1.0\n";
        material << "  OcclusionStrength: 1.0\n";
        material << "  Textures:\n";
        material << "    Albedo: 0\n";
        material << "    Normal: 0\n";
        material << "    MetallicRoughness: 0\n";
        material << "    Occlusion: 0\n";
        material << "    Emission: 0\n";
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

    const auto materialHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/Editable.lunamat");
    if (!materialHandle.isValid()) {
        std::cerr << "Failed to resolve material handle.\n";
        return 1;
    }

    tooling::ToolContext context;
    tooling::CommandArgs materialArgs;
    materialArgs.set("material", materialHandle);
    materialArgs.set("shading_model", static_cast<uint64_t>(renderer::interface::ShadingModel::Unlit));
    materialArgs.set("albedo_r", 0.1);
    materialArgs.set("albedo_g", 0.2);
    materialArgs.set("albedo_b", 0.3);
    materialArgs.set("albedo_a", 0.4);
    materialArgs.set("metallic", 1.5);
    materialArgs.set("roughness", -0.25);
    materialArgs.set("emission_r", 0.5);
    materialArgs.set("emission_g", 0.6);
    materialArgs.set("emission_b", 0.7);
    materialArgs.set("emission_strength", -2.0);
    materialArgs.set("normal_scale", -4.0);
    materialArgs.set("occlusion_strength", 2.0);
    materialArgs.set("albedo_texture", textureHandle);
    materialArgs.set("emission_texture", textureHandle);

    const auto setMaterialResult =
        tooling::CommandRegistry::get().execute(tooling::SetMaterialParametersCommandId, context, materialArgs);
    if (!setMaterialResult.success) {
        std::cerr << "asset.set_material_parameters failed: " << setMaterialResult.message << "\n";
        return 1;
    }

    const auto* material = asset::AssetManager::get().getAsset<renderer::interface::Material>(materialHandle);
    if (material == nullptr || material->parameters == nullptr ||
        material->parameters->shading_model != renderer::interface::ShadingModel::Unlit ||
        !nearlyEqual(material->parameters->albedo.r, 0.1f) || !nearlyEqual(material->parameters->albedo.g, 0.2f) ||
        !nearlyEqual(material->parameters->albedo.b, 0.3f) || !nearlyEqual(material->parameters->albedo.a, 0.4f) ||
        !nearlyEqual(material->parameters->metallic, 1.0f) || !nearlyEqual(material->parameters->roughness, 0.0f) ||
        !nearlyEqual(material->parameters->emission.r, 0.5f) || !nearlyEqual(material->parameters->emission.g, 0.6f) ||
        !nearlyEqual(material->parameters->emission.b, 0.7f) ||
        !nearlyEqual(material->parameters->emission_strength, 0.0f) ||
        !nearlyEqual(material->parameters->normal_scale, 0.0f) ||
        !nearlyEqual(material->parameters->occlusion_strength, 1.0f) ||
        material->parameters->albedo_texture != textureHandle ||
        material->parameters->emission_texture != textureHandle) {
        std::cerr << "asset.set_material_parameters did not update material parameters as expected.\n";
        return 1;
    }

    tooling::CommandArgs saveMaterialArgs;
    saveMaterialArgs.set("material", materialHandle);
    const auto saveMaterialResult =
        tooling::CommandRegistry::get().execute(tooling::SaveMaterialCommandId, context, saveMaterialArgs);
    if (!saveMaterialResult.success) {
        std::cerr << "asset.save_material failed: " << saveMaterialResult.message << "\n";
        return 1;
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        std::cerr << "Failed to reload test project assets after material save.\n";
        return 1;
    }

    const auto* reloadedMaterial = asset::AssetManager::get().getAsset<renderer::interface::Material>(materialHandle);
    if (reloadedMaterial == nullptr || reloadedMaterial->parameters == nullptr ||
        reloadedMaterial->parameters->shading_model != renderer::interface::ShadingModel::Unlit ||
        !nearlyEqual(reloadedMaterial->parameters->albedo.r, 0.1f) ||
        !nearlyEqual(reloadedMaterial->parameters->metallic, 1.0f) ||
        !nearlyEqual(reloadedMaterial->parameters->roughness, 0.0f) ||
        !nearlyEqual(reloadedMaterial->parameters->occlusion_strength, 1.0f) ||
        reloadedMaterial->parameters->albedo_texture != textureHandle ||
        reloadedMaterial->parameters->emission_texture != textureHandle) {
        std::cerr << "asset.save_material did not persist material parameters.\n";
        return 1;
    }

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

    tooling::CommandArgs createClipArgs;
    createClipArgs.set("target_directory", std::filesystem::path{"Assets"});
    const auto createClipResult =
        tooling::CommandRegistry::get().execute(tooling::CreateSpriteAnimationClipCommandId, context, createClipArgs);
    if (!createClipResult.success) {
        std::cerr << "asset.create_sprite_animation_clip failed: " << createClipResult.message << "\n";
        return 1;
    }
    const auto* clipAsset = createClipResult.get<asset::AssetHandle>("created_asset");
    const auto* clipPath = createClipResult.get<std::filesystem::path>("created_path");
    if (clipAsset == nullptr || !clipAsset->isValid() || clipPath == nullptr || !std::filesystem::exists(*clipPath) ||
        asset::AssetManager::get().getAsset<animation::SpriteAnimationClip>(*clipAsset) == nullptr) {
        std::cerr << "asset.create_sprite_animation_clip did not create a loadable clip asset.\n";
        return 1;
    }

    tooling::CommandArgs createFilledClipArgs;
    createFilledClipArgs.set("target_directory", std::filesystem::path{"Assets"});
    createFilledClipArgs.set("name", std::string{"Idle"});
    createFilledClipArgs.set("fps", 8.0);
    createFilledClipArgs.set("loop", false);
    createFilledClipArgs.set("frames", std::vector<asset::AssetHandle>{*createdAsset, *createdAsset});
    const auto createFilledClipResult = tooling::CommandRegistry::get().execute(
        tooling::CreateSpriteAnimationClipCommandId, context, createFilledClipArgs);
    if (!createFilledClipResult.success) {
        std::cerr << "asset.create_sprite_animation_clip with frames failed: " << createFilledClipResult.message
                  << "\n";
        return 1;
    }
    const auto* filledClipAsset = createFilledClipResult.get<asset::AssetHandle>("created_asset");
    if (filledClipAsset == nullptr || !filledClipAsset->isValid()) {
        std::cerr << "asset.create_sprite_animation_clip with frames did not report a created asset.\n";
        return 1;
    }
    const auto* filledClip = asset::AssetManager::get().getAsset<animation::SpriteAnimationClip>(*filledClipAsset);
    if (filledClip == nullptr || filledClip->frames.size() != 2 || !nearlyEqual(filledClip->fps, 8.0f) ||
        filledClip->loop || filledClip->frames[0].sprite != *createdAsset ||
        !nearlyEqual(filledClip->frames[0].duration, 0.125f)) {
        std::cerr << "asset.create_sprite_animation_clip with frames created unexpected clip data.\n";
        return 1;
    }

    tooling::CommandArgs saveClipArgs;
    saveClipArgs.set("clip", *filledClipAsset);
    saveClipArgs.set("fps", 4.0);
    saveClipArgs.set("loop", true);
    saveClipArgs.set("frames", std::vector<asset::AssetHandle>{*createdAsset});
    const auto saveClipResult =
        tooling::CommandRegistry::get().execute(tooling::SaveSpriteAnimationClipCommandId, context, saveClipArgs);
    if (!saveClipResult.success) {
        std::cerr << "asset.save_sprite_animation_clip failed: " << saveClipResult.message << "\n";
        return 1;
    }
    filledClip = asset::AssetManager::get().getAsset<animation::SpriteAnimationClip>(*filledClipAsset);
    if (filledClip == nullptr || filledClip->frames.size() != 1 || !nearlyEqual(filledClip->fps, 4.0f) ||
        !filledClip->loop || filledClip->frames[0].sprite != *createdAsset ||
        !nearlyEqual(filledClip->frames[0].duration, 0.25f)) {
        std::cerr << "asset.save_sprite_animation_clip did not persist updated clip data.\n";
        return 1;
    }

    tooling::CommandArgs createControllerArgs;
    createControllerArgs.set("target_directory", std::filesystem::path{"Assets"});
    const auto createControllerResult = tooling::CommandRegistry::get().execute(
        tooling::CreateSpriteAnimatorControllerCommandId, context, createControllerArgs);
    if (!createControllerResult.success) {
        std::cerr << "asset.create_sprite_animator_controller failed: " << createControllerResult.message << "\n";
        return 1;
    }
    const auto* controllerAsset = createControllerResult.get<asset::AssetHandle>("created_asset");
    const auto* controllerPath = createControllerResult.get<std::filesystem::path>("created_path");
    if (controllerAsset == nullptr || !controllerAsset->isValid() || controllerPath == nullptr ||
        !std::filesystem::exists(*controllerPath) ||
        asset::AssetManager::get().getAsset<animation::SpriteAnimatorController>(*controllerAsset) == nullptr) {
        std::cerr << "asset.create_sprite_animator_controller did not create a loadable controller asset.\n";
        return 1;
    }

    tooling::CommandArgs createFilledControllerArgs;
    createFilledControllerArgs.set("target_directory", std::filesystem::path{"Assets"});
    createFilledControllerArgs.set("name", std::string{"IdleAnimator"});
    createFilledControllerArgs.set("state_name", std::string{"Idle"});
    createFilledControllerArgs.set("clip", *filledClipAsset);
    createFilledControllerArgs.set("loop", false);
    const auto createFilledControllerResult = tooling::CommandRegistry::get().execute(
        tooling::CreateSpriteAnimatorControllerCommandId, context, createFilledControllerArgs);
    if (!createFilledControllerResult.success) {
        std::cerr << "asset.create_sprite_animator_controller with clip failed: "
                  << createFilledControllerResult.message << "\n";
        return 1;
    }
    const auto* filledControllerAsset = createFilledControllerResult.get<asset::AssetHandle>("created_asset");
    if (filledControllerAsset == nullptr || !filledControllerAsset->isValid()) {
        std::cerr << "asset.create_sprite_animator_controller with clip did not report a created asset.\n";
        return 1;
    }
    const auto* filledController =
        asset::AssetManager::get().getAsset<animation::SpriteAnimatorController>(*filledControllerAsset);
    if (filledController == nullptr || filledController->entry_state != "Idle" ||
        filledController->states.size() != 1 || filledController->states[0].clip != *filledClipAsset ||
        filledController->states[0].loop) {
        std::cerr << "asset.create_sprite_animator_controller with clip created unexpected controller data.\n";
        return 1;
    }

    tooling::CommandArgs spriteArgs;
    spriteArgs.set("sprite", *createdAsset);
    spriteArgs.set("texture", textureHandle);
    spriteArgs.set("source_x", uint64_t{1});
    spriteArgs.set("source_y", uint64_t{0});
    spriteArgs.set("source_width", uint64_t{1});
    spriteArgs.set("source_height", uint64_t{2});
    spriteArgs.set("pivot_x", 0.25);
    spriteArgs.set("pivot_y", 0.75);
    spriteArgs.set("pixels_per_unit", -4.0);
    spriteArgs.set("flip_y", false);
    spriteArgs.set("color_space", static_cast<uint64_t>(asset::SpriteColorSpace::Linear));
    const auto setSpriteResult =
        tooling::CommandRegistry::get().execute(tooling::SetSpriteParametersCommandId, context, spriteArgs);
    if (!setSpriteResult.success) {
        std::cerr << "asset.set_sprite_parameters failed: " << setSpriteResult.message << "\n";
        return 1;
    }

    sprite = asset::AssetManager::get().getAsset<asset::Sprite>(*createdAsset);
    if (sprite == nullptr || sprite->source_rect.x != 1 || sprite->source_rect.y != 0 ||
        sprite->source_rect.width != 1 || sprite->source_rect.height != 2 || !nearlyEqual(sprite->pivot.x, 0.25f) ||
        !nearlyEqual(sprite->pivot.y, 0.75f) || !nearlyEqual(sprite->pixels_per_unit, 0.0001f) || sprite->flip_y ||
        sprite->color_space != asset::SpriteColorSpace::Linear) {
        std::cerr << "asset.set_sprite_parameters did not update sprite parameters as expected.\n";
        return 1;
    }

    tooling::CommandArgs saveSpriteArgs;
    saveSpriteArgs.set("sprite", *createdAsset);
    const auto saveSpriteResult =
        tooling::CommandRegistry::get().execute(tooling::SaveSpriteCommandId, context, saveSpriteArgs);
    if (!saveSpriteResult.success) {
        std::cerr << "asset.save_sprite failed: " << saveSpriteResult.message << "\n";
        return 1;
    }

    tooling::CommandArgs textureArgs;
    textureArgs.set("texture", textureHandle);
    textureArgs.set("generate_mipmaps", false);
    textureArgs.set("color_space", static_cast<uint64_t>(renderer::interface::TextureColorSpace::Linear));
    textureArgs.set("min_filter", static_cast<uint64_t>(renderer::interface::TextureFilter::Nearest));
    textureArgs.set("mag_filter", static_cast<uint64_t>(renderer::interface::TextureFilter::Nearest));
    textureArgs.set("mip_filter", static_cast<uint64_t>(renderer::interface::TextureMipFilter::None));
    textureArgs.set("address_u", static_cast<uint64_t>(renderer::interface::TextureAddressMode::ClampToEdge));
    textureArgs.set("address_v", static_cast<uint64_t>(renderer::interface::TextureAddressMode::MirroredRepeat));
    textureArgs.set("address_w", static_cast<uint64_t>(renderer::interface::TextureAddressMode::ClampToEdge));
    const auto setTextureResult =
        tooling::CommandRegistry::get().execute(tooling::SetTextureImportSettingsCommandId, context, textureArgs);
    if (!setTextureResult.success) {
        std::cerr << "asset.set_texture_import_settings failed: " << setTextureResult.message << "\n";
        return 1;
    }

    const auto* texture = asset::AssetManager::get().getAsset<renderer::interface::Texture>(textureHandle);
    if (texture == nullptr || texture->getImportSettings().generate_mipmaps ||
        texture->getImportSettings().color_space != renderer::interface::TextureColorSpace::Linear ||
        texture->getImportSettings().sampler.min_filter != renderer::interface::TextureFilter::Nearest ||
        texture->getImportSettings().sampler.mag_filter != renderer::interface::TextureFilter::Nearest ||
        texture->getImportSettings().sampler.mip_filter != renderer::interface::TextureMipFilter::None ||
        texture->getImportSettings().sampler.address_u != renderer::interface::TextureAddressMode::ClampToEdge ||
        texture->getImportSettings().sampler.address_v != renderer::interface::TextureAddressMode::MirroredRepeat ||
        texture->getImportSettings().sampler.address_w != renderer::interface::TextureAddressMode::ClampToEdge) {
        std::cerr << "asset.set_texture_import_settings did not update texture settings as expected.\n";
        return 1;
    }

    tooling::CommandArgs saveTextureArgs;
    saveTextureArgs.set("texture", textureHandle);
    const auto saveTextureResult =
        tooling::CommandRegistry::get().execute(tooling::SaveTextureImportSettingsCommandId, context, saveTextureArgs);
    if (!saveTextureResult.success) {
        std::cerr << "asset.save_texture_import_settings failed: " << saveTextureResult.message << "\n";
        return 1;
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        std::cerr << "Failed to reload test project assets after sprite/texture save.\n";
        return 1;
    }

    const auto* reloadedSprite = asset::AssetManager::get().getAsset<asset::Sprite>(*createdAsset);
    if (reloadedSprite == nullptr || reloadedSprite->source_rect.x != 1 || reloadedSprite->source_rect.width != 1 ||
        !nearlyEqual(reloadedSprite->pivot.x, 0.25f) || !nearlyEqual(reloadedSprite->pivot.y, 0.75f) ||
        !nearlyEqual(reloadedSprite->pixels_per_unit, 0.0001f) || reloadedSprite->flip_y ||
        reloadedSprite->color_space != asset::SpriteColorSpace::Linear) {
        std::cerr << "asset.save_sprite did not persist sprite parameters.\n";
        return 1;
    }

    const auto* reloadedTexture = asset::AssetManager::get().getAsset<renderer::interface::Texture>(textureHandle);
    if (reloadedTexture == nullptr || reloadedTexture->getImportSettings().generate_mipmaps ||
        reloadedTexture->getImportSettings().color_space != renderer::interface::TextureColorSpace::Linear ||
        reloadedTexture->getImportSettings().sampler.min_filter != renderer::interface::TextureFilter::Nearest ||
        reloadedTexture->getImportSettings().sampler.mip_filter != renderer::interface::TextureMipFilter::None ||
        reloadedTexture->getImportSettings().sampler.address_v !=
            renderer::interface::TextureAddressMode::MirroredRepeat) {
        std::cerr << "asset.save_texture_import_settings did not persist texture settings.\n";
        return 1;
    }

    std::cout << "Tooling asset commands edited and saved material, sprite, and texture assets.\n";
    return 0;
}
