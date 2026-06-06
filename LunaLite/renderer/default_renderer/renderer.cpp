#include "../../core/log.h"
#include "environment_map_cache.h"
#include "gbuffer_resource.h"
#include "material_gpu_cache.h"
#include "passes/debug_line_pass.h"
#include "passes/geometry_pass.h"
#include "passes/lighting_pass.h"
#include "passes/skybox_pass.h"
#include "renderer.h"
#include "renderer_pipeline_resources.h"
#include "texture_gpu_cache.h"

#include <algorithm>
#include <memory>

namespace lunalite::renderer {

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

    m_geometry_pass = std::make_unique<GeometryPass>(*m_device,
                                                     *m_cmd,
                                                     m_pipeline_resources->geometryPipeline(),
                                                     m_pipeline_resources->geometryBindGroup(),
                                                     m_pipeline_resources->frameUniformBuffer(),
                                                     m_pipeline_resources->objectUniformBuffer(),
                                                     *m_material_gpu_cache,
                                                     m_frameUniforms,
                                                     m_frame_uniforms_dirty);
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
    m_geometry_pass.reset();
    m_environment_map_cache.reset();
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

    m_cmd->begin();
    m_geometry_pass->begin(gbuffer);
}

void Renderer::endFrame()
{
    const auto& gbuffer = m_gbuffer->get();
    m_geometry_pass->end();
    m_lighting_pass->execute(gbuffer, m_environment_map_cache->bindGroup());

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
