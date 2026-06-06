#include "../../core/log.h"
#include "../interface/frame_render_data.h"
#include "environment_map_cache.h"
#include "gbuffer_resource.h"
#include "material_gpu_cache.h"
#include "passes/debug_line_pass.h"
#include "passes/geometry_pass.h"
#include "passes/lighting_pass.h"
#include "passes/shadow_pass.h"
#include "passes/skybox_pass.h"
#include "renderer.h"
#include "renderer_pipeline_resources.h"
#include "shadow_map_resource.h"
#include "texture_gpu_cache.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <memory>
#include <vector>

namespace lunalite::renderer {
namespace {

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const auto length = glm::length(value);
    if (length <= 0.0001f) {
        return fallback;
    }

    return value / length;
}

struct CameraFrustumPlanes {
    std::array<glm::vec3, 4> near_corners{};
    std::array<glm::vec3, 4> far_corners{};
    bool perspective{true};
    float near_distance{0.1f};
    float far_distance{100.0f};
};

CameraFrustumPlanes cameraFrustumPlanesView(const glm::mat4& projection)
{
    const auto invProjection = glm::inverse(projection);
    const std::array<glm::vec2, 4> ndcCorners = {
        glm::vec2{-1.0f, -1.0f},
        glm::vec2{1.0f, -1.0f},
        glm::vec2{-1.0f, 1.0f},
        glm::vec2{1.0f, 1.0f},
    };

    CameraFrustumPlanes planes;
    planes.perspective = std::abs(projection[3][3]) <= 0.0001f;
    for (size_t i = 0; i < ndcCorners.size(); ++i) {
        const auto nearClip = glm::vec4{ndcCorners[i], -1.0f, 1.0f};
        const auto farClip = glm::vec4{ndcCorners[i], 1.0f, 1.0f};
        const auto nearView = invProjection * nearClip;
        const auto farView = invProjection * farClip;
        planes.near_corners[i] = glm::vec3{nearView} / nearView.w;
        planes.far_corners[i] = glm::vec3{farView} / farView.w;
    }

    planes.near_distance = std::numeric_limits<float>::max();
    planes.far_distance = 0.0f;
    for (size_t i = 0; i < ndcCorners.size(); ++i) {
        const auto nearDistance = std::max(-planes.near_corners[i].z, 0.0001f);
        const auto farDistance = std::max(-planes.far_corners[i].z, nearDistance + 0.0001f);
        planes.near_distance = std::min(planes.near_distance, nearDistance);
        planes.far_distance = std::max(planes.far_distance, farDistance);
    }

    return planes;
}

std::array<glm::vec3, 8> cameraFrustumSliceCornersWorld(const interface::CameraData& camera,
                                                        const CameraFrustumPlanes& planes,
                                                        float near_distance,
                                                        float far_distance)
{
    const auto invView = glm::inverse(camera.view);
    std::array<glm::vec3, 8> corners{};
    for (size_t i = 0; i < planes.near_corners.size(); ++i) {
        glm::vec3 nearView;
        glm::vec3 farView;
        if (planes.perspective) {
            const auto ray = planes.far_corners[i] / std::max(-planes.far_corners[i].z, 0.0001f);
            nearView = ray * near_distance;
            farView = ray * far_distance;
        } else {
            nearView = glm::vec3{planes.near_corners[i].x, planes.near_corners[i].y, -near_distance};
            farView = glm::vec3{planes.far_corners[i].x, planes.far_corners[i].y, -far_distance};
        }

        corners[i] = glm::vec3{invView * glm::vec4{nearView, 1.0f}};
        corners[i + 4] = glm::vec3{invView * glm::vec4{farView, 1.0f}};
    }

    return corners;
}

auto snapProjectionToShadowTexels(glm::mat4 light_projection, const glm::mat4& light_view, uint32_t shadow_map_size)
    -> glm::mat4
{
    const auto safeShadowMapSize = std::max(shadow_map_size, 1u);
    const auto lightViewProjection = light_projection * light_view;
    const auto shadowMapScale = static_cast<float>(safeShadowMapSize) * 0.5f;
    const auto shadowOrigin = lightViewProjection * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f} * shadowMapScale;
    const glm::vec2 roundedOrigin{std::round(shadowOrigin.x), std::round(shadowOrigin.y)};
    const auto roundOffset = (roundedOrigin - glm::vec2{shadowOrigin}) * (2.0f / static_cast<float>(safeShadowMapSize));

    light_projection[3][0] += roundOffset.x;
    light_projection[3][1] += roundOffset.y;
    return light_projection;
}

bool overlapsLightSpaceXY(const interface::AABB& lhs, const interface::AABB& rhs)
{
    if (!lhs.valid || !rhs.valid) {
        return false;
    }

    return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x && lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y;
}

glm::mat4 directionalShadowLightViewProjection(const std::array<glm::vec3, 8>& corners,
                                               const glm::vec3& light_direction,
                                               float cascade_far_distance,
                                               uint32_t shadow_map_size,
                                               const std::vector<interface::MeshDrawCommand>& mesh_commands,
                                               float caster_depth_padding)
{
    glm::vec3 center{0.0f};
    for (const auto& corner : corners) {
        center += corner;
    }
    center /= static_cast<float>(corners.size());

    const auto lightDir = safeNormalize(light_direction, glm::vec3{0.0f, -1.0f, 0.0f});
    const auto lightPosition = center - lightDir * std::max(cascade_far_distance, 1.0f);
    auto up = glm::vec3{0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(up, lightDir)) > 0.95f) {
        up = glm::vec3{1.0f, 0.0f, 0.0f};
    }

    const auto lightView = glm::lookAt(lightPosition, center, up);
    interface::AABB receiverBounds;
    for (const auto& corner : corners) {
        const auto lightSpaceCorner = glm::vec3{lightView * glm::vec4{corner, 1.0f}};
        receiverBounds.include(lightSpaceCorner);
    }

    auto minBounds = receiverBounds.min;
    auto maxBounds = receiverBounds.max;
    for (const auto& meshCommand : mesh_commands) {
        const auto casterBounds = meshCommand.world_aabb.transformed(lightView);
        if (!overlapsLightSpaceXY(casterBounds, receiverBounds)) {
            continue;
        }

        minBounds.z = std::min(minBounds.z, casterBounds.min.z);
        maxBounds.z = std::max(maxBounds.z, casterBounds.max.z);
    }

    const auto depthPadding = std::max(caster_depth_padding, 0.0f);
    minBounds.z -= depthPadding;
    maxBounds.z += depthPadding;

    auto lightProjection = glm::ortho(minBounds.x, maxBounds.x, minBounds.y, maxBounds.y, -maxBounds.z, -minBounds.z);
    lightProjection = snapProjectionToShadowTexels(lightProjection, lightView, shadow_map_size);
    return lightProjection * lightView;
}

ShadowCascadeData directionalShadowCascades(const interface::CameraData& camera,
                                            const interface::RenderDirectionalLight& light,
                                            uint32_t shadow_map_size,
                                            uint32_t cascade_count,
                                            const std::vector<interface::MeshDrawCommand>& mesh_commands)
{
    ShadowCascadeData cascadeData;
    cascadeData.count = std::clamp(cascade_count, 1u, MaxShadowCascades);

    const auto planes = cameraFrustumPlanesView(camera.projection);
    const auto nearDistance = std::max(planes.near_distance, 0.0001f);
    const auto farDistance =
        std::max(std::min(planes.far_distance, std::max(light.shadow.max_distance, 1.0f)), nearDistance + 0.0001f);
    const auto splitLambda = std::clamp(light.shadow.cascade_split_lambda, 0.0f, 1.0f);

    float previousSplit = nearDistance;
    for (uint32_t cascadeIndex = 0; cascadeIndex < cascadeData.count; ++cascadeIndex) {
        const auto splitFraction = static_cast<float>(cascadeIndex + 1u) / static_cast<float>(cascadeData.count);
        const auto uniformSplit = nearDistance + (farDistance - nearDistance) * splitFraction;
        const auto logarithmicSplit = nearDistance * std::pow(farDistance / nearDistance, splitFraction);
        const auto splitDistance = cascadeIndex + 1u == cascadeData.count
                                       ? farDistance
                                       : glm::mix(uniformSplit, logarithmicSplit, splitLambda);
        const auto corners = cameraFrustumSliceCornersWorld(camera, planes, previousSplit, splitDistance);

        auto& cascade = cascadeData.cascades[cascadeIndex];
        cascade.light_view_projection = directionalShadowLightViewProjection(corners,
                                                                             light.direction,
                                                                             splitDistance,
                                                                             shadow_map_size,
                                                                             mesh_commands,
                                                                             light.shadow.cascade_caster_depth_padding);
        cascade.split_depth = splitDistance;
        previousSplit = splitDistance;
    }

    return cascadeData;
}

bool shouldRenderShadowMap(const interface::FrameRenderData& frame)
{
    return frame.lighting.directional_light_count > 0 && frame.lighting.directional_light.shadow.enabled &&
           !frame.meshes.empty();
}

ShadowLightingUniforms makeShadowLightingUniforms(const ShadowCascadeData& cascade_data,
                                                  const interface::RenderShadowSettings& settings,
                                                  uint32_t shadow_map_size,
                                                  bool enabled)
{
    constexpr uint32_t maxPcfRadius = 4;
    const auto safeShadowMapSize = std::max(shadow_map_size, 1u);
    const auto texelSize = 1.0f / static_cast<float>(safeShadowMapSize);

    ShadowLightingUniforms uniforms;
    const auto cascadeCount = std::min(cascade_data.count, MaxShadowCascades);
    const auto uniformCascadeCount = std::max(cascadeCount, 1u);
    for (uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex) {
        uniforms.lightViewProjections[cascadeIndex] = cascade_data.cascades[cascadeIndex].light_view_projection;
        uniforms.cascadeSplits[cascadeIndex] = cascade_data.cascades[cascadeIndex].split_depth;
    }
    uniforms.texelSizeBiasNormalBias =
        glm::vec4{texelSize, texelSize, std::max(settings.bias, 0.0f), std::max(settings.normal_bias, 0.0f)};
    uniforms.cascadeBlendPadding = glm::vec4{std::max(settings.cascade_seam_blend, 0.0f), 0.0f, 0.0f, 0.0f};
    uniforms.enabledPcfRadiusCascadeCountPadding = glm::uvec4{
        enabled ? 1u : 0u,
        std::min(settings.pcf_radius, maxPcfRadius),
        uniformCascadeCount,
        0u,
    };
    return uniforms;
}

} // namespace

Renderer::Renderer(rhi::Device& device, rhi::Swapchain& swapchain)
    : m_device(&device),
      m_swapchain(&swapchain)
{
    m_command_list = m_device->createCommandList();
    m_cmd = m_device->getCommandList(m_command_list);
    m_upload_command_list = m_device->createCommandList();
    m_compute_command_list = m_device->createCommandList();
    m_compute_cmd = m_device->getCommandList(m_compute_command_list);
    LUNA_ASSERT(m_command_list, "Failed to create default renderer command list.");
    LUNA_ASSERT(m_cmd != nullptr, "Default renderer command list is null.");
    LUNA_ASSERT(m_upload_command_list, "Failed to create default renderer upload command list.");
    LUNA_ASSERT(m_compute_command_list, "Failed to create default renderer compute command list.");
    LUNA_ASSERT(m_compute_cmd != nullptr, "Default renderer compute command list is null.");

    m_pipeline_resources = std::make_unique<RendererPipelineResources>(*m_device);

    m_texture_gpu_cache = std::make_unique<TextureGpuCache>(*m_device, m_upload_command_list);
    m_material_gpu_cache = std::make_unique<MaterialGpuCache>(
        *m_device, m_pipeline_resources->materialTextureBindGroupLayout(), *m_texture_gpu_cache);
    m_gbuffer = std::make_unique<GBufferResource>(
        *m_device, m_pipeline_resources->lightingBindGroupLayout(), m_pipeline_resources->frameUniformBuffer());
    m_environment_map_cache =
        std::make_unique<EnvironmentMapCache>(*m_device,
                                              m_upload_command_list,
                                              m_compute_command_list,
                                              *m_compute_cmd,
                                              m_pipeline_resources->environmentBindGroupLayout(),
                                              m_pipeline_resources->environmentComputeBindGroupLayout(),
                                              m_pipeline_resources->environmentCubemapPipeline(),
                                              m_pipeline_resources->environmentIrradiancePipeline(),
                                              m_pipeline_resources->environmentPrefilterPipeline(),
                                              *m_texture_gpu_cache);
    m_shadow_map =
        std::make_unique<ShadowMapResource>(*m_device, m_pipeline_resources->shadowLightingBindGroupLayout());

    m_geometry_pass = std::make_unique<GeometryPass>(*m_device,
                                                     *m_cmd,
                                                     m_pipeline_resources->geometryPipeline(),
                                                     m_pipeline_resources->geometryBindGroup(),
                                                     m_pipeline_resources->frameUniformBuffer(),
                                                     m_pipeline_resources->objectUniformBuffer(),
                                                     *m_material_gpu_cache,
                                                     m_frameUniforms,
                                                     m_frame_uniforms_dirty);
    m_shadow_pass = std::make_unique<ShadowPass>(
        *m_device, *m_cmd, m_pipeline_resources->shadowPipeline(), m_pipeline_resources->shadowBindGroupLayout());
    m_debug_line_pass = std::make_unique<DebugLinePass>(*m_device,
                                                        *m_cmd,
                                                        m_pipeline_resources->linePipeline(),
                                                        m_pipeline_resources->geometryPipeline(),
                                                        m_pipeline_resources->geometryBindGroup(),
                                                        m_pipeline_resources->frameUniformBuffer(),
                                                        m_pipeline_resources->objectUniformBuffer(),
                                                        m_frameUniforms,
                                                        m_frame_uniforms_dirty);
    m_lighting_pass = std::make_unique<LightingPass>(*m_cmd, m_pipeline_resources->lightingPipeline());
    m_skybox_pass = std::make_unique<SkyboxPass>(
        *m_cmd, m_pipeline_resources->skyboxPipeline(), m_pipeline_resources->geometryBindGroup());

    LUNA_ASSERT(m_environment_map_cache->bindGroup(), "Failed to create environment bind group.");
    LUNA_CORE_DEBUG("Default renderer initialized");
}

Renderer::~Renderer()
{
    m_skybox_pass.reset();
    m_lighting_pass.reset();
    m_debug_line_pass.reset();
    m_shadow_pass.reset();
    m_geometry_pass.reset();
    m_environment_map_cache.reset();
    m_shadow_map.reset();
    m_gbuffer.reset();
    m_material_gpu_cache.reset();
    m_texture_gpu_cache.reset();
    m_pipeline_resources.reset();

    if (m_command_list) {
        m_device->destroyCommandList(m_command_list);
        m_command_list = {};
        m_cmd = nullptr;
    }
    if (m_compute_command_list) {
        m_device->destroyCommandList(m_compute_command_list);
        m_compute_command_list = {};
        m_compute_cmd = nullptr;
    }
    if (m_upload_command_list) {
        m_device->destroyCommandList(m_upload_command_list);
        m_upload_command_list = {};
    }
}

void Renderer::beginFrame()
{
    m_stats = diagnostics::RenderStats{};
    m_gbuffer->ensure(m_swapchain->getWidth(), m_swapchain->getHeight());
    const auto& gbuffer = m_gbuffer->get();
    LUNA_ASSERT(gbuffer.lighting_bind_group, "GBuffer is not initialized.");
    LUNA_ASSERT(m_environment_map_cache->bindGroup(), "Environment bind group is not initialized.");
    LUNA_ASSERT(m_shadow_map->lightingBindGroup(), "Shadow lighting bind group is not initialized.");

    m_shadow_map->updateLightingUniforms(makeShadowLightingUniforms(
        ShadowCascadeData{}, interface::RenderShadowSettings{}, m_shadow_map->size(), false));
    m_cmd->begin();
    m_geometry_pass_recorded_this_frame = false;
}

void Renderer::endFrame()
{
    const auto& gbuffer = m_gbuffer->get();
    if (m_geometry_pass_recorded_this_frame) {
        m_geometry_pass->end();
        m_geometry_pass_recorded_this_frame = false;
    }

    if (m_lighting_pass->execute(gbuffer,
                                 m_environment_map_cache->bindGroup(),
                                 m_shadow_map->lightingBindGroup(),
                                 m_shadow_map->texture())) {
        m_stats.lighting_draw_calls += 1;
    }

    if (m_frameUniforms.environmentIntensity > 0.0f) {
        if (m_skybox_pass->execute(gbuffer, m_environment_map_cache->bindGroup())) {
            m_stats.skybox_draw_calls += 1;
        }
    }

    m_stats.draw_calls_total = m_stats.geometry_draw_calls + m_stats.shadow_draw_calls + m_stats.debug_line_draw_calls +
                               m_stats.lighting_draw_calls + m_stats.skybox_draw_calls;

    m_lighting_pass->transitionFinalColorForSampling(gbuffer);

    m_cmd->end();
    m_device->submit(m_command_list);
}

void Renderer::resize(uint32_t width, uint32_t height)
{
    m_gbuffer->ensure(width, height);
}

void Renderer::renderFrame(const interface::FrameRenderData& frame)
{
    m_stats.mesh_commands =
        static_cast<uint32_t>(std::min<size_t>(frame.meshes.size(), std::numeric_limits<uint32_t>::max()));
    m_stats.debug_lines =
        static_cast<uint32_t>(std::min<size_t>(frame.debug_lines.size(), std::numeric_limits<uint32_t>::max()));

    setLighting(frame.lighting);
    setViewProjection(frame.camera.view, frame.camera.projection, frame.camera.position, frame.camera.exposure);

    if (shouldRenderShadowMap(frame)) {
        m_shadow_map->ensure(frame.lighting.directional_light.shadow);
        const auto cascadeData = directionalShadowCascades(frame.camera,
                                                           frame.lighting.directional_light,
                                                           m_shadow_map->size(),
                                                           m_shadow_map->cascadeCount(),
                                                           frame.meshes);
        m_stats.shadow_draw_calls += m_shadow_pass->execute(*m_shadow_map, cascadeData, frame.meshes);
        m_shadow_map->updateLightingUniforms(makeShadowLightingUniforms(
            cascadeData, frame.lighting.directional_light.shadow, m_shadow_map->size(), true));
    } else {
        m_shadow_map->updateLightingUniforms(makeShadowLightingUniforms(
            ShadowCascadeData{}, frame.lighting.directional_light.shadow, m_shadow_map->size(), false));
    }

    const auto& gbuffer = m_gbuffer->get();
    m_geometry_pass->begin(gbuffer);
    m_geometry_pass_recorded_this_frame = true;

    for (const auto& meshCommand : frame.meshes) {
        m_stats.geometry_draw_calls += m_geometry_pass->renderMesh(meshCommand);
    }

    for (const auto& lineCommand : frame.debug_lines) {
        if (m_debug_line_pass->renderLine(lineCommand)) {
            m_stats.debug_line_draw_calls += 1;
        }
    }
}

const interface::FrameImage& Renderer::getFrameImage() const
{
    return m_gbuffer->getFrameImage();
}

const diagnostics::RenderStats& Renderer::getStats() const
{
    return m_stats;
}

void Renderer::setViewProjection(const glm::mat4& view,
                                 const glm::mat4& proj,
                                 const glm::vec3& cameraPos,
                                 float exposure)
{
    m_frameUniforms.view = view;
    m_frameUniforms.projection = proj;
    m_frameUniforms.inverseViewProjection = glm::inverse(proj * view);
    m_frameUniforms.cameraPos = cameraPos;
    m_frameUniforms.exposure = std::max(exposure, 0.0f);
    m_frame_uniforms_dirty = true;
}

void Renderer::setLighting(const interface::RenderLighting& lighting)
{
    m_frameUniforms.directionalLightCount = lighting.directional_light_count > 0 ? 1u : 0u;
    const auto* environment = m_environment_map_cache->getOrCreate(lighting.environment_map);
    const auto& activeEnvironment = environment != nullptr ? *environment : m_environment_map_cache->fallback();

    m_frameUniforms.environmentIntensity =
        environment != nullptr ? std::max(lighting.environment_intensity, 0.0f) : 0.0f;
    m_frameUniforms.maxPrefilterMip = static_cast<float>(lastMipLevel(activeEnvironment.prefilter_mip_count));
    m_environment_map_cache->updateBindGroup(activeEnvironment);

    if (m_frameUniforms.directionalLightCount > 0) {
        m_frameUniforms.lightDir = lighting.directional_light.direction;
        m_frameUniforms.lightRadiance = lighting.directional_light.radiance;
    } else {
        m_frameUniforms.lightDir = {0.0f, -1.0f, 0.0f};
        m_frameUniforms.lightRadiance = {0.0f, 0.0f, 0.0f};
    }

    m_frame_uniforms_dirty = true;
}

} // namespace lunalite::renderer
