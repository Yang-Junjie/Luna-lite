#pragma once
#include "TinyRHI/interface/device.h"

namespace lunalite::renderer {

class RendererPipelineResources {
public:
    explicit RendererPipelineResources(rhi::Device& device);
    ~RendererPipelineResources();

    RendererPipelineResources(const RendererPipelineResources&) = delete;
    RendererPipelineResources& operator=(const RendererPipelineResources&) = delete;

    rhi::BindGroupLayoutHandle materialTextureBindGroupLayout() const
    {
        return m_material_texture_bind_group_layout;
    }

    rhi::BindGroupLayoutHandle lightingBindGroupLayout() const
    {
        return m_lighting_bind_group_layout;
    }

    rhi::BindGroupLayoutHandle environmentBindGroupLayout() const
    {
        return m_environment_bind_group_layout;
    }

    rhi::BindGroupLayoutHandle environmentComputeBindGroupLayout() const
    {
        return m_environment_compute_bind_group_layout;
    }

    rhi::BindGroupLayoutHandle shadowBindGroupLayout() const
    {
        return m_shadow_bind_group_layout;
    }

    rhi::BindGroupLayoutHandle shadowLightingBindGroupLayout() const
    {
        return m_shadow_lighting_bind_group_layout;
    }

    rhi::PipelineHandle geometryPipeline() const
    {
        return m_geometry_pipeline;
    }

    rhi::PipelineHandle linePipeline() const
    {
        return m_line_pipeline;
    }

    rhi::PipelineHandle lightingPipeline() const
    {
        return m_lighting_pipeline;
    }

    rhi::PipelineHandle skyboxPipeline() const
    {
        return m_skybox_pipeline;
    }

    rhi::PipelineHandle environmentCubemapPipeline() const
    {
        return m_environment_cubemap_pipeline;
    }

    rhi::PipelineHandle environmentIrradiancePipeline() const
    {
        return m_environment_irradiance_pipeline;
    }

    rhi::PipelineHandle environmentPrefilterPipeline() const
    {
        return m_environment_prefilter_pipeline;
    }

    rhi::PipelineHandle shadowPipeline() const
    {
        return m_shadow_pipeline;
    }

    rhi::BufferHandle frameUniformBuffer() const
    {
        return m_frame_uniform_buffer;
    }

    rhi::BufferHandle objectUniformBuffer() const
    {
        return m_object_uniform_buffer;
    }

    rhi::BindGroupHandle geometryBindGroup() const
    {
        return m_geometry_bind_group;
    }

private:
    void createShaders();
    void createBindGroupLayouts();
    void createPipelineLayouts();
    void createPipelines();
    void createUniformBuffers();
    void createGeometryBindGroup();
    void validateShaders() const;
    void validateBindGroupLayouts() const;
    void validatePipelineLayouts() const;
    void validatePipelines() const;
    void validateUniformBindings() const;
    void destroy();

    rhi::Device* m_device{nullptr};

    rhi::ShaderHandle m_geometry_vertex_shader{};
    rhi::ShaderHandle m_geometry_fragment_shader{};
    rhi::ShaderHandle m_line_vertex_shader{};
    rhi::ShaderHandle m_line_fragment_shader{};
    rhi::ShaderHandle m_lighting_vertex_shader{};
    rhi::ShaderHandle m_lighting_fragment_shader{};
    rhi::ShaderHandle m_skybox_vertex_shader{};
    rhi::ShaderHandle m_skybox_fragment_shader{};
    rhi::ShaderHandle m_environment_cubemap_compute_shader{};
    rhi::ShaderHandle m_environment_irradiance_compute_shader{};
    rhi::ShaderHandle m_environment_prefilter_compute_shader{};
    rhi::ShaderHandle m_shadow_vertex_shader{};
    rhi::ShaderHandle m_shadow_fragment_shader{};

    rhi::BindGroupLayoutHandle m_geometry_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_material_texture_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_lighting_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_environment_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_environment_compute_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_shadow_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_shadow_lighting_bind_group_layout{};

    rhi::PipelineLayoutHandle m_geometry_pipeline_layout{};
    rhi::PipelineLayoutHandle m_lighting_pipeline_layout{};
    rhi::PipelineLayoutHandle m_skybox_pipeline_layout{};
    rhi::PipelineLayoutHandle m_environment_compute_pipeline_layout{};
    rhi::PipelineLayoutHandle m_shadow_pipeline_layout{};

    rhi::PipelineHandle m_geometry_pipeline{};
    rhi::PipelineHandle m_line_pipeline{};
    rhi::PipelineHandle m_lighting_pipeline{};
    rhi::PipelineHandle m_skybox_pipeline{};
    rhi::PipelineHandle m_environment_cubemap_pipeline{};
    rhi::PipelineHandle m_environment_irradiance_pipeline{};
    rhi::PipelineHandle m_environment_prefilter_pipeline{};
    rhi::PipelineHandle m_shadow_pipeline{};

    rhi::BufferHandle m_frame_uniform_buffer{};
    rhi::BufferHandle m_object_uniform_buffer{};
    rhi::BindGroupHandle m_geometry_bind_group{};
};

} // namespace lunalite::renderer
