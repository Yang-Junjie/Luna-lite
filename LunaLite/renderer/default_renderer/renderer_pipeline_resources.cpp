#include "../../core/log.h"
#include "../interface/mesh.h"
#include "renderer_common.h"
#include "renderer_pipeline_resources.h"

#include <cstddef>

namespace lunalite::renderer {

RendererPipelineResources::RendererPipelineResources(rhi::Device& device)
    : m_device(&device)
{
    createShaders();
    createBindGroupLayouts();
    createPipelineLayouts();
    createPipelines();
    createUniformBuffers();
    createGeometryBindGroup();

    validateShaders();
    validateBindGroupLayouts();
    validatePipelineLayouts();
    validatePipelines();
    validateUniformBindings();
}

RendererPipelineResources::~RendererPipelineResources()
{
    destroy();
}

void RendererPipelineResources::createShaders()
{
    const auto geometryVertexShaderSource = loadDefaultRendererShader("geometry.vert");
    const auto geometryFragmentShaderSource = loadDefaultRendererShader("geometry.frag");
    const auto lineVertexShaderSource = loadDefaultRendererShader("line.vert");
    const auto lineFragmentShaderSource = loadDefaultRendererShader("line.frag");
    const auto spriteVertexShaderSource = loadDefaultRendererShader("sprite.vert");
    const auto spriteFragmentShaderSource = loadDefaultRendererShader("sprite.frag");
    const auto lightingVertexShaderSource = loadDefaultRendererShader("lighting.vert");
    const auto lightingFragmentShaderSource = loadDefaultRendererShader("lighting.frag");
    const auto skyboxVertexShaderSource = loadDefaultRendererShader("skybox.vert");
    const auto skyboxFragmentShaderSource = loadDefaultRendererShader("skybox.frag");
    const auto environmentCubemapComputeShaderSource = loadDefaultRendererShader("environment_cubemap.comp");
    const auto environmentIrradianceComputeShaderSource = loadDefaultRendererShader("environment_irradiance.comp");
    const auto environmentPrefilterComputeShaderSource = loadDefaultRendererShader("environment_prefilter.comp");
    const auto shadowVertexShaderSource = loadDefaultRendererShader("shadow.vert");
    const auto shadowFragmentShaderSource = loadDefaultRendererShader("shadow.frag");

    m_geometry_vertex_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = geometryVertexShaderSource.c_str(),
    });
    m_geometry_fragment_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = geometryFragmentShaderSource.c_str(),
    });
    m_line_vertex_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = lineVertexShaderSource.c_str(),
    });
    m_line_fragment_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = lineFragmentShaderSource.c_str(),
    });
    m_sprite_vertex_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = spriteVertexShaderSource.c_str(),
    });
    m_sprite_fragment_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = spriteFragmentShaderSource.c_str(),
    });
    m_lighting_vertex_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = lightingVertexShaderSource.c_str(),
    });
    m_lighting_fragment_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = lightingFragmentShaderSource.c_str(),
    });
    m_skybox_vertex_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = skyboxVertexShaderSource.c_str(),
    });
    m_skybox_fragment_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = skyboxFragmentShaderSource.c_str(),
    });
    m_environment_cubemap_compute_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = environmentCubemapComputeShaderSource.c_str(),
    });
    m_environment_irradiance_compute_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = environmentIrradianceComputeShaderSource.c_str(),
    });
    m_environment_prefilter_compute_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = environmentPrefilterComputeShaderSource.c_str(),
    });
    m_shadow_vertex_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = shadowVertexShaderSource.c_str(),
    });
    m_shadow_fragment_shader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = shadowFragmentShaderSource.c_str(),
    });
}

void RendererPipelineResources::createBindGroupLayouts()
{
    m_geometry_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Vertex) |
                              rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 1,
                    .type = rhi::BindingType::UniformBuffer,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Vertex) |
                              rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
            },
    });

    m_material_texture_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 1,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 2,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 3,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 4,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
            },
    });

    m_sprite_texture_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
            },
    });

    m_lighting_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 1,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 2,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 3,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 4,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 5,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
            },
    });

    m_environment_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 1,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 2,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 3,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
            },
    });

    m_environment_compute_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Compute),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 1,
                    .type = rhi::BindingType::StorageTexture,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Compute),
                },
            },
    });

    m_shadow_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Vertex),
                },
            },
    });

    m_shadow_lighting_bind_group_layout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 1,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
            },
    });
}

void RendererPipelineResources::createPipelineLayouts()
{
    m_geometry_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_geometry_bind_group_layout, m_material_texture_bind_group_layout},
    });
    m_sprite_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_geometry_bind_group_layout, m_sprite_texture_bind_group_layout},
    });
    m_lighting_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_lighting_bind_group_layout,
                               m_environment_bind_group_layout,
                               m_shadow_lighting_bind_group_layout},
    });
    m_skybox_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_geometry_bind_group_layout, m_environment_bind_group_layout},
    });
    m_environment_compute_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_environment_compute_bind_group_layout},
        .push_constants =
            {
                rhi::PushConstantRange{
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Compute),
                    .offset = 0,
                    .size = sizeof(glm::vec4),
                },
            },
    });
    m_shadow_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_shadow_bind_group_layout},
        .push_constants =
            {
                rhi::PushConstantRange{
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Vertex),
                    .offset = 0,
                    .size = sizeof(glm::mat4),
                },
            },
    });
}

void RendererPipelineResources::createPipelines()
{
    m_geometry_pipeline = m_device
                              ->createPipeline(
                                  rhi::PipelineDesc{
                                      .topology = rhi::PrimitiveTopology::Triangle,
                                      .vertex_input =
                                          rhi::VertexInputDesc{
                                              .buffers =
                                                  {
                                                      rhi::VertexBufferLayoutDesc{
                                                          .binding = 0,
                                                          .stride = sizeof(interface::Vertex),
                                                          .attributes =
                                                              {
                                                                  rhi::VertexAttributeDesc{
                                                                      .location = 0,
                                                                      .format = rhi::VertexFormat::Float3,
                                                                      .offset = offsetof(interface::Vertex, position),
                                                                  },
                                                                  rhi::VertexAttributeDesc{
                                                                      .location = 1,
                                                                      .format = rhi::VertexFormat::Float3,
                                                                      .offset = offsetof(interface::Vertex, normal),
                                                                  },
                                                                  rhi::VertexAttributeDesc{
                                                                      .location = 2,
                                                                      .format = rhi::VertexFormat::Float2,
                                                                      .offset = offsetof(interface::Vertex, uv),
                                                                  },
                                                                  rhi::VertexAttributeDesc{
                                                                      .location = 3,
                                                                      .format = rhi::VertexFormat::Float3,
                                                                      .offset = offsetof(interface::Vertex, tangent),
                                                                  },
                                                                  rhi::VertexAttributeDesc{
                                                                      .location = 4,
                                                                      .format = rhi::VertexFormat::Float3,
                                                                      .offset = offsetof(interface::Vertex, bitangent),
                                                                  },
                                                              },
                                                      },
                                                  },
                                          },
                                      .layout = m_geometry_pipeline_layout,
                                      .vertex_shader = m_geometry_vertex_shader,
                                      .fragment_shader = m_geometry_fragment_shader,
                                      .render_target_state =
                                          rhi::RenderTargetState{
                                              .color_targets =
                                                  {
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA16F},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA16F},
                                                  },
                                              .has_depth_stencil = true,
                                              .depth_stencil_format = rhi::TextureFormat::Depth24Stencil8,
                                          },
                                  });

    const auto lineVertexInput = rhi::VertexInputDesc{
        .buffers =
            {
                rhi::VertexBufferLayoutDesc{
                    .binding = 0,
                    .stride = sizeof(LineVertex),
                    .attributes =
                        {
                            rhi::VertexAttributeDesc{
                                .location = 0,
                                .format = rhi::VertexFormat::Float3,
                                .offset = offsetof(LineVertex, position),
                            },
                            rhi::VertexAttributeDesc{
                                .location = 1,
                                .format = rhi::VertexFormat::Float3,
                                .offset = offsetof(LineVertex, color),
                            },
                        },
                },
            },
    };
    const auto lineColorTarget = rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm};

    m_line_depth_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Line,
        .vertex_input = lineVertexInput,
        .layout = m_geometry_pipeline_layout,
        .vertex_shader = m_line_vertex_shader,
        .fragment_shader = m_line_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets = {lineColorTarget},
                .has_depth_stencil = true,
                .depth_stencil_format = rhi::TextureFormat::Depth24Stencil8,
            },
        .depth_state =
            rhi::DepthState{
                .enabled = true,
                .write_enabled = false,
                .compare = rhi::CompareOp::LessOrEqual,
            },
    });
    m_line_overlay_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Line,
        .vertex_input = lineVertexInput,
        .layout = m_geometry_pipeline_layout,
        .vertex_shader = m_line_vertex_shader,
        .fragment_shader = m_line_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets = {lineColorTarget},
            },
        .depth_state =
            rhi::DepthState{
                .enabled = false,
                .write_enabled = false,
            },
    });

    const auto spriteVertexInput = rhi::VertexInputDesc{
        .buffers =
            {
                rhi::VertexBufferLayoutDesc{
                    .binding = 0,
                    .stride = sizeof(SpriteVertex),
                    .attributes =
                        {
                            rhi::VertexAttributeDesc{
                                .location = 0,
                                .format = rhi::VertexFormat::Float3,
                                .offset = offsetof(SpriteVertex, position),
                            },
                            rhi::VertexAttributeDesc{
                                .location = 1,
                                .format = rhi::VertexFormat::Float2,
                                .offset = offsetof(SpriteVertex, uv),
                            },
                            rhi::VertexAttributeDesc{
                                .location = 2,
                                .format = rhi::VertexFormat::Float4,
                                .offset = offsetof(SpriteVertex, color),
                            },
                        },
                },
            },
    };
    const auto spriteColorTarget = rhi::ColorTargetState{
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .blend =
            rhi::BlendState{
                .enabled = true,
                .src_color = rhi::BlendFactor::SrcAlpha,
                .dst_color = rhi::BlendFactor::OneMinusSrcAlpha,
                .color_op = rhi::BlendOp::Add,
                .src_alpha = rhi::BlendFactor::One,
                .dst_alpha = rhi::BlendFactor::OneMinusSrcAlpha,
                .alpha_op = rhi::BlendOp::Add,
            },
    };
    m_sprite_depth_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_input = spriteVertexInput,
        .layout = m_sprite_pipeline_layout,
        .vertex_shader = m_sprite_vertex_shader,
        .fragment_shader = m_sprite_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets = {spriteColorTarget},
                .has_depth_stencil = true,
                .depth_stencil_format = rhi::TextureFormat::Depth24Stencil8,
            },
        .depth_state =
            rhi::DepthState{
                .enabled = true,
                .write_enabled = false,
                .compare = rhi::CompareOp::LessOrEqual,
            },
    });
    m_sprite_overlay_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_input = spriteVertexInput,
        .layout = m_sprite_pipeline_layout,
        .vertex_shader = m_sprite_vertex_shader,
        .fragment_shader = m_sprite_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets = {spriteColorTarget},
            },
        .depth_state =
            rhi::DepthState{
                .enabled = false,
                .write_enabled = false,
            },
    });

    m_lighting_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_input = rhi::VertexInputDesc{},
        .layout = m_lighting_pipeline_layout,
        .vertex_shader = m_lighting_vertex_shader,
        .fragment_shader = m_lighting_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets =
                    {
                        rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
                    },
            },
        .depth_state =
            rhi::DepthState{
                .enabled = false,
                .write_enabled = false,
            },
    });

    m_skybox_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_input = rhi::VertexInputDesc{},
        .layout = m_skybox_pipeline_layout,
        .vertex_shader = m_skybox_vertex_shader,
        .fragment_shader = m_skybox_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets =
                    {
                        rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
                    },
                .has_depth_stencil = true,
                .depth_stencil_format = rhi::TextureFormat::Depth24Stencil8,
            },
        .depth_state =
            rhi::DepthState{
                .enabled = true,
                .write_enabled = false,
                .compare = rhi::CompareOp::LessOrEqual,
            },
    });

    m_shadow_pipeline =
        m_device->createPipeline(
            rhi::PipelineDesc{
                .topology = rhi::PrimitiveTopology::Triangle,
                .vertex_input =
                    rhi::VertexInputDesc{
                        .buffers =
                            {
                                rhi::VertexBufferLayoutDesc{
                                    .binding = 0,
                                    .stride = sizeof(interface::Vertex),
                                    .attributes =
                                        {
                                            rhi::VertexAttributeDesc{
                                                .location = 0,
                                                .format = rhi::VertexFormat::Float3,
                                                .offset = offsetof(interface::Vertex, position),
                                            },
                                        },
                                },
                            },
                    },
                .layout = m_shadow_pipeline_layout,
                .vertex_shader = m_shadow_vertex_shader,
                .fragment_shader = m_shadow_fragment_shader,
                .render_target_state =
                    rhi::RenderTargetState{
                        .has_depth_stencil = true,
                        .depth_stencil_format = rhi::TextureFormat::Depth32F,
                    },
                .depth_state =
                    rhi::DepthState{
                        .enabled = true,
                        .write_enabled = true,
                        .compare = rhi::CompareOp::Less,
                    },
            });

    m_environment_cubemap_pipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = m_environment_compute_pipeline_layout,
        .compute_shader = m_environment_cubemap_compute_shader,
    });
    m_environment_irradiance_pipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = m_environment_compute_pipeline_layout,
        .compute_shader = m_environment_irradiance_compute_shader,
    });
    m_environment_prefilter_pipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = m_environment_compute_pipeline_layout,
        .compute_shader = m_environment_prefilter_compute_shader,
    });
}

void RendererPipelineResources::createUniformBuffers()
{
    m_frame_uniform_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(FrameUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::UniformRead,
    });
    m_object_uniform_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(ObjectUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::UniformRead,
    });
}

void RendererPipelineResources::createGeometryBindGroup()
{
    m_geometry_bind_group = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_geometry_bind_group_layout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_frame_uniform_buffer,
                            .offset = 0,
                            .size = sizeof(FrameUniforms),
                        },
                },
                rhi::BindGroupEntry{
                    .binding = 1,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_object_uniform_buffer,
                            .offset = 0,
                            .size = sizeof(ObjectUniforms),
                        },
                },
            },
    });
}

void RendererPipelineResources::validateShaders() const
{
    LUNA_ASSERT(m_geometry_vertex_shader, "Failed to create geometry vertex shader.");
    LUNA_ASSERT(m_geometry_fragment_shader, "Failed to create geometry fragment shader.");
    LUNA_ASSERT(m_line_vertex_shader, "Failed to create line vertex shader.");
    LUNA_ASSERT(m_line_fragment_shader, "Failed to create line fragment shader.");
    LUNA_ASSERT(m_sprite_vertex_shader, "Failed to create sprite vertex shader.");
    LUNA_ASSERT(m_sprite_fragment_shader, "Failed to create sprite fragment shader.");
    LUNA_ASSERT(m_lighting_vertex_shader, "Failed to create lighting vertex shader.");
    LUNA_ASSERT(m_lighting_fragment_shader, "Failed to create lighting fragment shader.");
    LUNA_ASSERT(m_skybox_vertex_shader, "Failed to create skybox vertex shader.");
    LUNA_ASSERT(m_skybox_fragment_shader, "Failed to create skybox fragment shader.");
    LUNA_ASSERT(m_environment_cubemap_compute_shader, "Failed to create environment cubemap compute shader.");
    LUNA_ASSERT(m_environment_irradiance_compute_shader, "Failed to create environment irradiance compute shader.");
    LUNA_ASSERT(m_environment_prefilter_compute_shader, "Failed to create environment prefilter compute shader.");
    LUNA_ASSERT(m_shadow_vertex_shader, "Failed to create shadow vertex shader.");
    LUNA_ASSERT(m_shadow_fragment_shader, "Failed to create shadow fragment shader.");
}

void RendererPipelineResources::validateBindGroupLayouts() const
{
    LUNA_ASSERT(m_geometry_bind_group_layout, "Failed to create geometry bind group layout.");
    LUNA_ASSERT(m_material_texture_bind_group_layout, "Failed to create material texture bind group layout.");
    LUNA_ASSERT(m_sprite_texture_bind_group_layout, "Failed to create sprite texture bind group layout.");
    LUNA_ASSERT(m_lighting_bind_group_layout, "Failed to create lighting bind group layout.");
    LUNA_ASSERT(m_environment_bind_group_layout, "Failed to create environment bind group layout.");
    LUNA_ASSERT(m_environment_compute_bind_group_layout, "Failed to create environment compute bind group layout.");
    LUNA_ASSERT(m_shadow_bind_group_layout, "Failed to create shadow bind group layout.");
    LUNA_ASSERT(m_shadow_lighting_bind_group_layout, "Failed to create shadow lighting bind group layout.");
}

void RendererPipelineResources::validatePipelineLayouts() const
{
    LUNA_ASSERT(m_geometry_pipeline_layout, "Failed to create geometry pipeline layout.");
    LUNA_ASSERT(m_sprite_pipeline_layout, "Failed to create sprite pipeline layout.");
    LUNA_ASSERT(m_lighting_pipeline_layout, "Failed to create lighting pipeline layout.");
    LUNA_ASSERT(m_skybox_pipeline_layout, "Failed to create skybox pipeline layout.");
    LUNA_ASSERT(m_environment_compute_pipeline_layout, "Failed to create environment compute pipeline layout.");
    LUNA_ASSERT(m_shadow_pipeline_layout, "Failed to create shadow pipeline layout.");
}

void RendererPipelineResources::validatePipelines() const
{
    LUNA_ASSERT(m_geometry_pipeline, "Failed to create geometry pipeline.");
    LUNA_ASSERT(m_lighting_pipeline, "Failed to create lighting pipeline.");
    LUNA_ASSERT(m_skybox_pipeline, "Failed to create skybox pipeline.");
    LUNA_ASSERT(m_line_depth_pipeline, "Failed to create depth-tested line pipeline.");
    LUNA_ASSERT(m_line_overlay_pipeline, "Failed to create overlay line pipeline.");
    LUNA_ASSERT(m_sprite_depth_pipeline, "Failed to create depth-tested sprite pipeline.");
    LUNA_ASSERT(m_sprite_overlay_pipeline, "Failed to create overlay sprite pipeline.");
    LUNA_ASSERT(m_environment_cubemap_pipeline, "Failed to create environment cubemap pipeline.");
    LUNA_ASSERT(m_environment_irradiance_pipeline, "Failed to create environment irradiance pipeline.");
    LUNA_ASSERT(m_environment_prefilter_pipeline, "Failed to create environment prefilter pipeline.");
    LUNA_ASSERT(m_shadow_pipeline, "Failed to create shadow pipeline.");
}

void RendererPipelineResources::validateUniformBindings() const
{
    LUNA_ASSERT(m_frame_uniform_buffer, "Failed to create frame uniform buffer.");
    LUNA_ASSERT(m_object_uniform_buffer, "Failed to create object uniform buffer.");
    LUNA_ASSERT(m_geometry_bind_group, "Failed to create geometry bind group.");
}

void RendererPipelineResources::destroy()
{
    if (m_geometry_bind_group) {
        m_device->destroyBindGroup(m_geometry_bind_group);
        m_geometry_bind_group = {};
    }
    if (m_object_uniform_buffer) {
        m_device->destroyBuffer(m_object_uniform_buffer);
        m_object_uniform_buffer = {};
    }
    if (m_frame_uniform_buffer) {
        m_device->destroyBuffer(m_frame_uniform_buffer);
        m_frame_uniform_buffer = {};
    }

    if (m_environment_prefilter_pipeline) {
        m_device->destroyPipeline(m_environment_prefilter_pipeline);
        m_environment_prefilter_pipeline = {};
    }
    if (m_shadow_pipeline) {
        m_device->destroyPipeline(m_shadow_pipeline);
        m_shadow_pipeline = {};
    }
    if (m_environment_irradiance_pipeline) {
        m_device->destroyPipeline(m_environment_irradiance_pipeline);
        m_environment_irradiance_pipeline = {};
    }
    if (m_environment_cubemap_pipeline) {
        m_device->destroyPipeline(m_environment_cubemap_pipeline);
        m_environment_cubemap_pipeline = {};
    }
    if (m_skybox_pipeline) {
        m_device->destroyPipeline(m_skybox_pipeline);
        m_skybox_pipeline = {};
    }
    if (m_lighting_pipeline) {
        m_device->destroyPipeline(m_lighting_pipeline);
        m_lighting_pipeline = {};
    }
    if (m_sprite_overlay_pipeline) {
        m_device->destroyPipeline(m_sprite_overlay_pipeline);
        m_sprite_overlay_pipeline = {};
    }
    if (m_sprite_depth_pipeline) {
        m_device->destroyPipeline(m_sprite_depth_pipeline);
        m_sprite_depth_pipeline = {};
    }
    if (m_line_overlay_pipeline) {
        m_device->destroyPipeline(m_line_overlay_pipeline);
        m_line_overlay_pipeline = {};
    }
    if (m_line_depth_pipeline) {
        m_device->destroyPipeline(m_line_depth_pipeline);
        m_line_depth_pipeline = {};
    }
    if (m_geometry_pipeline) {
        m_device->destroyPipeline(m_geometry_pipeline);
        m_geometry_pipeline = {};
    }

    if (m_environment_compute_pipeline_layout) {
        m_device->destroyPipelineLayout(m_environment_compute_pipeline_layout);
        m_environment_compute_pipeline_layout = {};
    }
    if (m_shadow_pipeline_layout) {
        m_device->destroyPipelineLayout(m_shadow_pipeline_layout);
        m_shadow_pipeline_layout = {};
    }
    if (m_skybox_pipeline_layout) {
        m_device->destroyPipelineLayout(m_skybox_pipeline_layout);
        m_skybox_pipeline_layout = {};
    }
    if (m_lighting_pipeline_layout) {
        m_device->destroyPipelineLayout(m_lighting_pipeline_layout);
        m_lighting_pipeline_layout = {};
    }
    if (m_sprite_pipeline_layout) {
        m_device->destroyPipelineLayout(m_sprite_pipeline_layout);
        m_sprite_pipeline_layout = {};
    }
    if (m_geometry_pipeline_layout) {
        m_device->destroyPipelineLayout(m_geometry_pipeline_layout);
        m_geometry_pipeline_layout = {};
    }

    if (m_environment_compute_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_environment_compute_bind_group_layout);
        m_environment_compute_bind_group_layout = {};
    }
    if (m_shadow_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_shadow_bind_group_layout);
        m_shadow_bind_group_layout = {};
    }
    if (m_shadow_lighting_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_shadow_lighting_bind_group_layout);
        m_shadow_lighting_bind_group_layout = {};
    }
    if (m_environment_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_environment_bind_group_layout);
        m_environment_bind_group_layout = {};
    }
    if (m_lighting_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_lighting_bind_group_layout);
        m_lighting_bind_group_layout = {};
    }
    if (m_sprite_texture_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_sprite_texture_bind_group_layout);
        m_sprite_texture_bind_group_layout = {};
    }
    if (m_material_texture_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_material_texture_bind_group_layout);
        m_material_texture_bind_group_layout = {};
    }
    if (m_geometry_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_geometry_bind_group_layout);
        m_geometry_bind_group_layout = {};
    }

    if (m_environment_prefilter_compute_shader) {
        m_device->destroyShader(m_environment_prefilter_compute_shader);
        m_environment_prefilter_compute_shader = {};
    }
    if (m_shadow_fragment_shader) {
        m_device->destroyShader(m_shadow_fragment_shader);
        m_shadow_fragment_shader = {};
    }
    if (m_shadow_vertex_shader) {
        m_device->destroyShader(m_shadow_vertex_shader);
        m_shadow_vertex_shader = {};
    }
    if (m_environment_irradiance_compute_shader) {
        m_device->destroyShader(m_environment_irradiance_compute_shader);
        m_environment_irradiance_compute_shader = {};
    }
    if (m_environment_cubemap_compute_shader) {
        m_device->destroyShader(m_environment_cubemap_compute_shader);
        m_environment_cubemap_compute_shader = {};
    }
    if (m_skybox_fragment_shader) {
        m_device->destroyShader(m_skybox_fragment_shader);
        m_skybox_fragment_shader = {};
    }
    if (m_skybox_vertex_shader) {
        m_device->destroyShader(m_skybox_vertex_shader);
        m_skybox_vertex_shader = {};
    }
    if (m_lighting_fragment_shader) {
        m_device->destroyShader(m_lighting_fragment_shader);
        m_lighting_fragment_shader = {};
    }
    if (m_lighting_vertex_shader) {
        m_device->destroyShader(m_lighting_vertex_shader);
        m_lighting_vertex_shader = {};
    }
    if (m_line_fragment_shader) {
        m_device->destroyShader(m_line_fragment_shader);
        m_line_fragment_shader = {};
    }
    if (m_sprite_fragment_shader) {
        m_device->destroyShader(m_sprite_fragment_shader);
        m_sprite_fragment_shader = {};
    }
    if (m_sprite_vertex_shader) {
        m_device->destroyShader(m_sprite_vertex_shader);
        m_sprite_vertex_shader = {};
    }
    if (m_line_vertex_shader) {
        m_device->destroyShader(m_line_vertex_shader);
        m_line_vertex_shader = {};
    }
    if (m_geometry_fragment_shader) {
        m_device->destroyShader(m_geometry_fragment_shader);
        m_geometry_fragment_shader = {};
    }
    if (m_geometry_vertex_shader) {
        m_device->destroyShader(m_geometry_vertex_shader);
        m_geometry_vertex_shader = {};
    }
}

} // namespace lunalite::renderer
