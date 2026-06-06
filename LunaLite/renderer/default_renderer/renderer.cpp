#include "../../core/log.h"
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

std::array<glm::vec3, 8> cameraFrustumCornersWorld(const interface::CameraData& camera, float shadow_distance)
{
    const auto invViewProjection = glm::inverse(camera.projection * camera.view);
    const std::array<glm::vec3, 8> ndcCorners = {
        glm::vec3{-1.0f, -1.0f, -1.0f},
        glm::vec3{1.0f, -1.0f, -1.0f},
        glm::vec3{-1.0f, 1.0f, -1.0f},
        glm::vec3{1.0f, 1.0f, -1.0f},
        glm::vec3{-1.0f, -1.0f, 1.0f},
        glm::vec3{1.0f, -1.0f, 1.0f},
        glm::vec3{-1.0f, 1.0f, 1.0f},
        glm::vec3{1.0f, 1.0f, 1.0f},
    };

    std::array<glm::vec3, 8> corners{};
    for (size_t i = 0; i < ndcCorners.size(); ++i) {
        const auto clip = glm::vec4{ndcCorners[i], 1.0f};
        const auto world = invViewProjection * clip;
        corners[i] = glm::vec3{world} / world.w;
    }

    for (size_t i = 4; i < corners.size(); ++i) {
        const auto fromCamera = corners[i] - camera.position;
        const auto distance = glm::length(fromCamera);
        if (distance > shadow_distance) {
            corners[i] = camera.position + safeNormalize(fromCamera, glm::vec3{0.0f, 0.0f, -1.0f}) * shadow_distance;
        }
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

glm::mat4 directionalShadowLightViewProjection(const interface::CameraData& camera,
                                               const interface::RenderDirectionalLight& light,
                                               uint32_t shadow_map_size)
{
    const auto lightDir = safeNormalize(light.direction, glm::vec3{0.0f, -1.0f, 0.0f});
    const auto shadowDistance = std::max(light.shadow.max_distance, 1.0f);
    const auto corners = cameraFrustumCornersWorld(camera, shadowDistance);

    glm::vec3 center{0.0f};
    for (const auto& corner : corners) {
        center += corner;
    }
    center /= static_cast<float>(corners.size());

    const auto lightPosition = center - lightDir * shadowDistance;
    auto up = glm::vec3{0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(up, lightDir)) > 0.95f) {
        up = glm::vec3{1.0f, 0.0f, 0.0f};
    }

    const auto lightView = glm::lookAt(lightPosition, center, up);
    glm::vec3 minBounds{std::numeric_limits<float>::max()};
    glm::vec3 maxBounds{-std::numeric_limits<float>::max()};
    for (const auto& corner : corners) {
        const auto lightSpaceCorner = glm::vec3{lightView * glm::vec4{corner, 1.0f}};
        minBounds = glm::min(minBounds, lightSpaceCorner);
        maxBounds = glm::max(maxBounds, lightSpaceCorner);
    }

    constexpr float depthPadding = 10.0f;
    minBounds.z -= depthPadding;
    maxBounds.z += depthPadding;

    auto lightProjection = glm::ortho(minBounds.x, maxBounds.x, minBounds.y, maxBounds.y, -maxBounds.z, -minBounds.z);
    lightProjection = snapProjectionToShadowTexels(lightProjection, lightView, shadow_map_size);
    return lightProjection * lightView;
}

bool shouldRenderShadowMap(const interface::FrameRenderData& frame)
{
    return frame.lighting.directional_light_count > 0 && frame.lighting.directional_light.shadow.enabled &&
           !frame.meshes.empty();
}

ShadowLightingUniforms makeShadowLightingUniforms(const glm::mat4& light_view_projection,
                                                  const interface::RenderShadowSettings& settings,
                                                  uint32_t shadow_map_size,
                                                  bool enabled)
{
    constexpr uint32_t maxPcfRadius = 4;
    const auto safeShadowMapSize = std::max(shadow_map_size, 1u);
    const auto texelSize = 1.0f / static_cast<float>(safeShadowMapSize);

    ShadowLightingUniforms uniforms;
    uniforms.lightViewProjection = light_view_projection;
    uniforms.texelSizeBiasNormalBias =
        glm::vec4{texelSize, texelSize, std::max(settings.bias, 0.0f), std::max(settings.normal_bias, 0.0f)};
    uniforms.enabledPcfRadiusPadding =
        glm::uvec4{enabled ? 1u : 0u, std::min(settings.pcf_radius, maxPcfRadius), 0u, 0u};
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
    m_gbuffer->ensure(m_swapchain->getWidth(), m_swapchain->getHeight());
    const auto& gbuffer = m_gbuffer->get();
    LUNA_ASSERT(gbuffer.lighting_bind_group, "GBuffer is not initialized.");
    LUNA_ASSERT(m_environment_map_cache->bindGroup(), "Environment bind group is not initialized.");
    LUNA_ASSERT(m_shadow_map->lightingBindGroup(), "Shadow lighting bind group is not initialized.");

    m_shadow_map->updateLightingUniforms(
        makeShadowLightingUniforms(glm::mat4{1.0f}, interface::RenderShadowSettings{}, m_shadow_map->size(), false));
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

    m_lighting_pass->execute(
        gbuffer, m_environment_map_cache->bindGroup(), m_shadow_map->lightingBindGroup(), m_shadow_map->texture());

    if (m_frameUniforms.environmentIntensity > 0.0f) {
        m_skybox_pass->execute(gbuffer, m_environment_map_cache->bindGroup());
    }

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
    setLighting(frame.lighting);
    setViewProjection(frame.camera.view, frame.camera.projection, frame.camera.position, frame.camera.exposure);

    if (shouldRenderShadowMap(frame)) {
        m_shadow_map->ensure(frame.lighting.directional_light.shadow);
        const auto lightViewProjection =
            directionalShadowLightViewProjection(frame.camera, frame.lighting.directional_light, m_shadow_map->size());
        m_shadow_pass->execute(*m_shadow_map, lightViewProjection, frame.meshes);
        m_shadow_map->updateLightingUniforms(makeShadowLightingUniforms(
            lightViewProjection, frame.lighting.directional_light.shadow, m_shadow_map->size(), true));
    } else {
        m_shadow_map->updateLightingUniforms(makeShadowLightingUniforms(
            glm::mat4{1.0f}, frame.lighting.directional_light.shadow, m_shadow_map->size(), false));
    }

    const auto& gbuffer = m_gbuffer->get();
    m_geometry_pass->begin(gbuffer);
    m_geometry_pass_recorded_this_frame = true;

    for (const auto& meshCommand : frame.meshes) {
        m_geometry_pass->renderMesh(meshCommand);
    }

    for (const auto& lineCommand : frame.debug_lines) {
        m_debug_line_pass->renderLine(lineCommand);
    }
}

const interface::FrameImage& Renderer::getFrameImage() const
{
    return m_gbuffer->getFrameImage();
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
