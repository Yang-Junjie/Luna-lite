#include "../asset/asset_database.h"
#include "../asset/sprite.h"
#include "../core/log.h"
#include "../renderer/interface/mesh.h"
#include "../renderer/interface/sprite_geometry.h"
#include "../renderer/interface/texture.h"
#include "components.h"
#include "scene_renderer.h"

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <utility>

namespace lunalite::scene {
namespace {
glm::vec3 directionalLightDirection(const glm::quat& rotation)
{
    const auto direction = rotation * glm::vec3{0.0f, -1.0f, 0.0f};
    const auto length = glm::length(direction);
    if (length <= 0.0001f) {
        return glm::vec3{0.0f, -1.0f, 0.0f};
    }

    return direction / length;
}

bool decomposePose(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation)
{
    translation = glm::vec3{matrix[3]};

    glm::vec3 basis0{matrix[0]};
    glm::vec3 basis1{matrix[1]};
    glm::vec3 basis2{matrix[2]};

    const auto scaleX = glm::length(basis0);
    const auto scaleY = glm::length(basis1);
    const auto scaleZ = glm::length(basis2);
    constexpr float minScale = 0.000001f;
    if (scaleX <= minScale || scaleY <= minScale || scaleZ <= minScale) {
        return false;
    }

    basis0 /= scaleX;
    basis1 /= scaleY;
    basis2 /= scaleZ;

    if (glm::determinant(glm::mat3{basis0, basis1, basis2}) < 0.0f) {
        if (scaleX >= scaleY && scaleX >= scaleZ) {
            basis0 = -basis0;
        } else if (scaleY >= scaleZ) {
            basis1 = -basis1;
        } else {
            basis2 = -basis2;
        }
    }

    rotation = glm::normalize(glm::quat_cast(glm::mat3{basis0, basis1, basis2}));
    return true;
}

renderer::interface::RenderShadowSettings toRenderShadowSettings(const ShadowSettings& shadow)
{
    return renderer::interface::RenderShadowSettings{
        .enabled = shadow.enabled,
        .map_size = std::max(shadow.map_size, 1u),
        .max_distance = std::max(shadow.max_distance, 0.0f),
        .bias = std::max(shadow.bias, 0.0f),
        .normal_bias = std::max(shadow.normal_bias, 0.0f),
        .pcf_radius = shadow.pcf_radius,
        .cascade_count = std::clamp(shadow.cascade_count, 1u, 4u),
        .cascade_split_lambda = std::clamp(shadow.cascade_split_lambda, 0.0f, 1.0f),
        .cascade_seam_blend = std::max(shadow.cascade_seam_blend, 0.0f),
        .cascade_caster_depth_padding = std::max(shadow.cascade_caster_depth_padding, 0.0f),
    };
}

renderer::interface::TextureColorSpace toTextureColorSpace(asset::SpriteColorSpace color_space)
{
    switch (color_space) {
        case asset::SpriteColorSpace::Linear:
            return renderer::interface::TextureColorSpace::Linear;
        case asset::SpriteColorSpace::SRGB:
        default:
            return renderer::interface::TextureColorSpace::SRGB;
    }
}

renderer::interface::SpriteDrawCommand makeSpriteDrawCommand(const SpriteRendererComponent& spriteRenderer,
                                                             const glm::mat4& transform)
{
    renderer::interface::SpriteDrawCommand command;
    command.transform = transform;
    command.color = spriteRenderer.color;
    command.sorting_layer = spriteRenderer.sorting_layer;
    command.order_in_layer = spriteRenderer.order_in_layer;
    command.depth_test = spriteRenderer.depth_test;

    const auto* sprite = asset::AssetDatabase::get().get<asset::Sprite>(spriteRenderer.sprite);
    if (sprite == nullptr) {
        LUNA_CORE_ERROR("Sprite renderer has a null sprite asset {}", spriteRenderer.sprite.toString());
        return command;
    }

    const auto* texture = asset::AssetDatabase::get().get<renderer::interface::Texture>(sprite->texture);
    if (texture == nullptr || texture->getWidth() == 0 || texture->getHeight() == 0) {
        LUNA_CORE_ERROR(
            "Sprite asset {} references an invalid texture {}", sprite->handle.toString(), sprite->texture.toString());
        return command;
    }

    const auto textureWidth = texture->getWidth();
    const auto textureHeight = texture->getHeight();
    const auto sourceX = std::min(sprite->source_rect.x, textureWidth);
    const auto sourceY = std::min(sprite->source_rect.y, textureHeight);
    const auto sourceWidth = std::min(
        sprite->source_rect.width == 0 ? textureWidth - sourceX : sprite->source_rect.width, textureWidth - sourceX);
    const auto sourceHeight =
        std::min(sprite->source_rect.height == 0 ? textureHeight - sourceY : sprite->source_rect.height,
                 textureHeight - sourceY);
    if (sourceWidth == 0 || sourceHeight == 0) {
        LUNA_CORE_ERROR("Sprite asset {} has an empty source rect", sprite->handle.toString());
        return command;
    }

    command.texture = sprite->texture;
    command.color_space = toTextureColorSpace(sprite->color_space);
    command.size =
        glm::vec2{static_cast<float>(sourceWidth), static_cast<float>(sourceHeight)} / sprite->pixels_per_unit;
    command.pivot = sprite->pivot;
    command.uv_rect = glm::vec4{
        static_cast<float>(sourceX) / static_cast<float>(textureWidth),
        static_cast<float>(sourceY) / static_cast<float>(textureHeight),
        static_cast<float>(sourceWidth) / static_cast<float>(textureWidth),
        static_cast<float>(sourceHeight) / static_cast<float>(textureHeight),
    };
    command.flip_y = sprite->flip_y;
    return command;
}
} // namespace

SceneRenderer::SceneRenderer() {}

void SceneRenderer::setViewportSize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    m_viewport_width = width;
    m_viewport_height = height;
}

void SceneRenderer::beginFrame(renderer::interface::FrameRenderData& frame)
{
    m_frame_data = &frame;
}

void SceneRenderer::endFrame()
{
    m_frame_data = nullptr;
}

void SceneRenderer::onRenderRuntime(const Scene& scene)
{
    LUNA_ASSERT(m_frame_data, "SceneRenderer has no frame render data.");

    const CameraComponent* cameraComponent = nullptr;
    Entity cameraEntity;
    const auto cameraView = scene.getRegistry().view<const TransformComponent, const CameraComponent>();
    for (const auto entity : cameraView) {
        const auto& candidate = cameraView.get<const CameraComponent>(entity);
        if (!candidate.primary && cameraComponent != nullptr) {
            continue;
        }

        cameraComponent = &candidate;
        cameraEntity = Entity{entity};
        if (candidate.primary) {
            break;
        }
    }

    if (cameraComponent == nullptr || !cameraEntity) {
        return;
    }

    const float aspectRatio = static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height);
    const auto cameraWorld = scene.getWorldTransform(cameraEntity);
    glm::vec3 cameraPosition{cameraWorld[3]};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
    if (decomposePose(cameraWorld, cameraPosition, cameraRotation)) {
        const auto cameraNoScale = glm::translate(glm::mat4{1.0f}, cameraPosition) * glm::mat4_cast(cameraRotation);
        const auto view = glm::inverse(cameraNoScale);
        const auto projection = cameraComponent->camera.getProjection(aspectRatio);
        renderScene(scene, view, projection, cameraPosition, cameraComponent->camera.getExposure(), *m_frame_data);
        return;
    }

    const auto view = glm::inverse(cameraWorld);
    const auto projection = cameraComponent->camera.getProjection(aspectRatio);
    renderScene(scene, view, projection, cameraPosition, cameraComponent->camera.getExposure(), *m_frame_data);
}

void SceneRenderer::onRenderEditor(const Scene& scene, const renderer::interface::Camera& camera)
{
    LUNA_ASSERT(m_frame_data, "SceneRenderer has no frame render data.");

    const float aspectRatio = static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height);
    renderScene(scene,
                camera.getView(),
                camera.getProjection(aspectRatio),
                camera.getPosition(),
                camera.getExposure(),
                *m_frame_data);
}

void SceneRenderer::renderScene(const Scene& scene,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                const glm::vec3& cameraPosition,
                                float exposure,
                                renderer::interface::FrameRenderData& frame)
{
    renderer::interface::RenderLighting lighting{};
    lighting.environment_map = scene.getSettings().environment_map;
    lighting.environment_intensity = scene.getSettings().environment_intensity;
    {
        const auto lightView = scene.getRegistry().view<const TransformComponent, const DirectionalLightComponent>();
        for (const auto entity : lightView) {
            const auto& light = lightView.get<const DirectionalLightComponent>(entity);
            const auto world = scene.getWorldTransform(Entity{entity});
            glm::vec3 translation{0.0f};
            glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
            if (decomposePose(world, translation, rotation)) {
                lighting.directional_light_count = 1;
                lighting.directional_light.direction = directionalLightDirection(rotation);
                lighting.directional_light.radiance = light.color * light.intensity;
                lighting.directional_light.shadow = toRenderShadowSettings(light.shadow);
                break;
            }

            lighting.directional_light_count = 1;
            lighting.directional_light.direction =
                directionalLightDirection(lightView.get<const TransformComponent>(entity).rotation);
            lighting.directional_light.radiance = light.color * light.intensity;
            lighting.directional_light.shadow = toRenderShadowSettings(light.shadow);
            break;
        }
    }

    frame.lighting = lighting;
    frame.camera.view = view;
    frame.camera.projection = projection;
    frame.camera.view_projection = projection * view;
    frame.camera.inverse_view_projection = glm::inverse(frame.camera.view_projection);
    frame.camera.position = cameraPosition;
    frame.camera.exposure = exposure;

    const auto meshView = scene.getRegistry().view<const TransformComponent, const MeshRendererComponent>();
    for (const auto entity : meshView) {
        const auto& meshRenderer = meshView.get<const MeshRendererComponent>(entity);
        if (!meshRenderer.mesh.isValid()) {
            continue;
        }

        const auto* mesh = asset::AssetDatabase::get().get<renderer::interface::Mesh>(meshRenderer.mesh);
        if (mesh == nullptr) {
            LUNA_CORE_ERROR("Entity {} has a null mesh asset", static_cast<uint32_t>(entity));
            continue;
        }

        renderer::interface::MeshDrawCommand meshCommand;
        meshCommand.mesh = meshRenderer.mesh;
        meshCommand.materials = meshRenderer.materials;
        meshCommand.transform = scene.getWorldTransform(Entity{entity});
        meshCommand.local_aabb = mesh->getLocalAABB(meshRenderer.submesh_start, meshRenderer.submesh_count);
        meshCommand.world_aabb = meshCommand.local_aabb.transformed(meshCommand.transform);
        meshCommand.cast_shadow = meshRenderer.cast_shadow;
        meshCommand.submesh_start = meshRenderer.submesh_start;
        meshCommand.submesh_count = meshRenderer.submesh_count;
        frame.meshes.push_back(std::move(meshCommand));
    }

    const auto spriteView = scene.getRegistry().view<const TransformComponent, const SpriteRendererComponent>();
    for (const auto entity : spriteView) {
        const auto& spriteRenderer = spriteView.get<const SpriteRendererComponent>(entity);
        if (!spriteRenderer.sprite.isValid()) {
            continue;
        }

        auto spriteCommand = makeSpriteDrawCommand(spriteRenderer, scene.getWorldTransform(Entity{entity}));
        if (!spriteCommand.texture.isValid()) {
            continue;
        }
        if (!renderer::interface::spriteIntersectsClipVolume(spriteCommand, frame.camera.view_projection)) {
            continue;
        }
        frame.sprites.push_back(std::move(spriteCommand));
    }
}
} // namespace lunalite::scene
