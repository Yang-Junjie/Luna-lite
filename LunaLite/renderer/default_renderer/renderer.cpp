#include "../../core/log.h"
#include "renderer.h"

#include <cstddef>
#include <cstdint>

#include <limits>

namespace lunalite::renderer {
namespace {

size_t growBufferCapacity(size_t current, size_t required)
{
    size_t capacity = current > 0 ? current : 256;
    while (capacity < required) {
        capacity *= 2;
    }

    return capacity;
}

constexpr const char* geometryVertexShaderSource = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(std140, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad0;
    vec3 lightDir;
    float _pad1;
    vec3 lightAmbient;
    float _pad2;
    vec3 lightDiffuse;
    float _pad3;
    vec3 lightSpecular;
    float _pad4;
    uint directionalLightCount;
    float _pad5;
    float _pad6;
    float _pad7;
    vec3 environmentAmbient;
    float _pad8;
};

layout(std140, binding = 1) uniform ObjectUniforms {
    mat4 model;
    mat4 normalMatrix;
};

out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
    vec4 worldPosition = model * vec4(aPosition, 1.0);
    vWorldPos = worldPosition.xyz;
    vNormal = normalize(mat3(normalMatrix) * aNormal);
    gl_Position = projection * view * worldPosition;
}
)";

constexpr const char* geometryFragmentShaderSource = R"(
#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gMaterial;

void main()
{
    gAlbedo = vec4(0.8, 0.65, 0.5, 1.0);
    gNormal = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    gMaterial = vec4(0.5, 0.5, 0.0, 1.0);
}
)";

constexpr const char* lightingVertexShaderSource = R"(
#version 450 core

out vec2 vUV;

void main()
{
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    const vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );

    vUV = uvs[gl_VertexID];
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)";

constexpr const char* lightingFragmentShaderSource = R"(
#version 450 core

layout(std140, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad0;
    vec3 lightDir;
    float _pad1;
    vec3 lightAmbient;
    float _pad2;
    vec3 lightDiffuse;
    float _pad3;
    vec3 lightSpecular;
    float _pad4;
    uint directionalLightCount;
    float _pad5;
    float _pad6;
    float _pad7;
    vec3 environmentAmbient;
    float _pad8;
};

layout(binding = 1) uniform sampler2D gAlbedoTexture;
layout(binding = 2) uniform sampler2D gNormalTexture;
layout(binding = 3) uniform sampler2D gMaterialTexture;
layout(binding = 4) uniform sampler2D gDepthTexture;

in vec2 vUV;
out vec4 outColor;

void main()
{
    vec3 albedo = texture(gAlbedoTexture, vUV).rgb;
    vec3 normal = normalize(texture(gNormalTexture, vUV).rgb * 2.0 - 1.0);
    vec3 material = texture(gMaterialTexture, vUV).rgb;

    vec3 color = environmentAmbient * albedo;
    if (directionalLightCount > 0u) {
        vec3 L = normalize(-lightDir);
        float diff = max(dot(normal, L), 0.0);

        vec3 ambient = lightAmbient * albedo;
        vec3 diffuse = lightDiffuse * diff * albedo;
        color += ambient + diffuse;
    }

    color += material.b * 0.0;
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
)";

} // namespace

Renderer::Renderer(rhi::Device& device, rhi::Swapchain& swapchain)
    : m_device(&device),
      m_swapchain(&swapchain)
{
    m_cmd = &m_device->getCommandList();

    const auto geometryVertexShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = geometryVertexShaderSource,
    });

    const auto geometryFragmentShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = geometryFragmentShaderSource,
    });

    const auto lightingVertexShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = lightingVertexShaderSource,
    });

    const auto lightingFragmentShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = lightingFragmentShaderSource,
    });

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
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Vertex),
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
            },
    });

    m_geometry_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_geometry_bind_group_layout},
    });

    m_lighting_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_lighting_bind_group_layout},
    });

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
                                                              },
                                                      },
                                                  },
                                          },
                                      .layout = m_geometry_pipeline_layout,
                                      .vertex_shader = geometryVertexShader,
                                      .fragment_shader = geometryFragmentShader,
                                      .render_target_state =
                                          rhi::RenderTargetState{
                                              .color_targets =
                                                  {
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA16F},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8},
                                                  },
                                              .has_depth_stencil = true,
                                              .depth_stencil_format = rhi::TextureFormat::Depth24Stencil8,
                                          },
                                  });

    m_lighting_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_input = rhi::VertexInputDesc{},
        .layout = m_lighting_pipeline_layout,
        .vertex_shader = lightingVertexShader,
        .fragment_shader = lightingFragmentShader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets =
                    {
                        rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8},
                    },
            },
        .depth_state =
            rhi::DepthState{
                .enabled = false,
                .write_enabled = false,
            },
    });

    m_frameUniformBuffer = m_device->createBuffer(
        rhi::BufferDesc{
            .size = sizeof(FrameUniforms),
            .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
            .memory = rhi::MemoryUsage::CpuToGpu,
        },
        nullptr);

    m_objectUniformBuffer = m_device->createBuffer(
        rhi::BufferDesc{
            .size = sizeof(ObjectUniforms),
            .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
            .memory = rhi::MemoryUsage::CpuToGpu,
        },
        nullptr);

    m_geometry_bind_group = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_geometry_bind_group_layout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_frameUniformBuffer,
                            .offset = 0,
                            .size = sizeof(FrameUniforms),
                        },
                },
                rhi::BindGroupEntry{
                    .binding = 1,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_objectUniformBuffer,
                            .offset = 0,
                            .size = sizeof(ObjectUniforms),
                        },
                },
            },
    });

    m_gbuffer_sampler = m_device->createSampler(rhi::SamplerDesc{
        .min_filter = rhi::FilterMode::Nearest,
        .mag_filter = rhi::FilterMode::Nearest,
        .address_u = rhi::AddressMode::ClampToEdge,
        .address_v = rhi::AddressMode::ClampToEdge,
    });

    LUNA_ASSERT(geometryVertexShader, "Failed to create geometry vertex shader.");
    LUNA_ASSERT(geometryFragmentShader, "Failed to create geometry fragment shader.");
    LUNA_ASSERT(lightingVertexShader, "Failed to create lighting vertex shader.");
    LUNA_ASSERT(lightingFragmentShader, "Failed to create lighting fragment shader.");
    LUNA_ASSERT(m_geometry_bind_group_layout, "Failed to create geometry bind group layout.");
    LUNA_ASSERT(m_lighting_bind_group_layout, "Failed to create lighting bind group layout.");
    LUNA_ASSERT(m_geometry_pipeline_layout, "Failed to create geometry pipeline layout.");
    LUNA_ASSERT(m_lighting_pipeline_layout, "Failed to create lighting pipeline layout.");
    LUNA_ASSERT(m_geometry_pipeline, "Failed to create geometry pipeline.");
    LUNA_ASSERT(m_lighting_pipeline, "Failed to create lighting pipeline.");
    LUNA_ASSERT(m_frameUniformBuffer, "Failed to create frame uniform buffer.");
    LUNA_ASSERT(m_objectUniformBuffer, "Failed to create object uniform buffer.");
    LUNA_ASSERT(m_geometry_bind_group, "Failed to create geometry bind group.");
    LUNA_ASSERT(m_gbuffer_sampler, "Failed to create GBuffer sampler.");
    LUNA_CORE_DEBUG("Default renderer initialized");
}

void Renderer::ensureGBuffer(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    if (m_gbuffer.width == width && m_gbuffer.height == height && m_gbuffer.lighting_bind_group) {
        return;
    }

    if (m_gbuffer.lighting_bind_group) {
        m_device->destroyBindGroup(m_gbuffer.lighting_bind_group);
    }

    const rhi::TextureViewHandle views[] = {
        m_gbuffer.albedo_view,
        m_gbuffer.normal_view,
        m_gbuffer.material_view,
        m_gbuffer.depth_view,
        m_gbuffer.final_color_view,
    };
    for (const auto view : views) {
        if (view) {
            m_device->destroyTextureView(view);
        }
    }

    const rhi::TextureHandle textures[] = {
        m_gbuffer.albedo_texture,
        m_gbuffer.normal_texture,
        m_gbuffer.material_texture,
        m_gbuffer.depth_texture,
        m_gbuffer.final_color_texture,
    };
    for (const auto texture : textures) {
        if (texture) {
            m_device->destroyTexture(texture);
        }
    }

    m_gbuffer = GBuffer{.width = width, .height = height};

    m_gbuffer.albedo_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
    });
    m_gbuffer.normal_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA16F,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
    });
    m_gbuffer.material_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
    });
    m_gbuffer.depth_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::Depth24Stencil8,
        .usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled,
    });
    m_gbuffer.final_color_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
    });

    m_gbuffer.albedo_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.albedo_texture,
        .format = rhi::TextureFormat::RGBA8,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.normal_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.normal_texture,
        .format = rhi::TextureFormat::RGBA16F,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.material_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.material_texture,
        .format = rhi::TextureFormat::RGBA8,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.depth_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.depth_texture,
        .format = rhi::TextureFormat::Depth24Stencil8,
        .aspect = rhi::TextureAspect::DepthStencil,
    });
    m_gbuffer.final_color_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.final_color_texture,
        .format = rhi::TextureFormat::RGBA8,
        .aspect = rhi::TextureAspect::Color,
    });

    m_gbuffer.lighting_bind_group = m_device
                                        ->createBindGroup(
                                            rhi::BindGroupDesc{
                                                .layout = m_lighting_bind_group_layout,
                                                .entries =
                                                    {
                                                        rhi::BindGroupEntry{
                                                            .binding = 0,
                                                            .type = rhi::BindingType::UniformBuffer,
                                                            .buffer =
                                                                rhi::BufferBinding{
                                                                    .buffer = m_frameUniformBuffer,
                                                                    .offset = 0,
                                                                    .size = sizeof(FrameUniforms),
                                                                },
                                                        },
                                                        rhi::BindGroupEntry{
                                                            .binding = 1,
                                                            .type = rhi::BindingType::CombinedImageSampler,
                                                            .texture_view = m_gbuffer.albedo_view,
                                                            .sampler = m_gbuffer_sampler,
                                                        },
                                                        rhi::BindGroupEntry{
                                                            .binding = 2,
                                                            .type = rhi::BindingType::CombinedImageSampler,
                                                            .texture_view = m_gbuffer.normal_view,
                                                            .sampler = m_gbuffer_sampler,
                                                        },
                                                        rhi::BindGroupEntry{
                                                            .binding = 3,
                                                            .type = rhi::BindingType::CombinedImageSampler,
                                                            .texture_view = m_gbuffer.material_view,
                                                            .sampler = m_gbuffer_sampler,
                                                        },
                                                        rhi::BindGroupEntry{
                                                            .binding = 4,
                                                            .type = rhi::BindingType::CombinedImageSampler,
                                                            .texture_view = m_gbuffer.depth_view,
                                                            .sampler = m_gbuffer_sampler,
                                                        },
                                                    },
                                            });

    LUNA_ASSERT(m_gbuffer.albedo_texture, "Failed to create GBuffer albedo texture.");
    LUNA_ASSERT(m_gbuffer.normal_texture, "Failed to create GBuffer normal texture.");
    LUNA_ASSERT(m_gbuffer.material_texture, "Failed to create GBuffer material texture.");
    LUNA_ASSERT(m_gbuffer.depth_texture, "Failed to create GBuffer depth texture.");
    LUNA_ASSERT(m_gbuffer.final_color_texture, "Failed to create GBuffer final color texture.");
    LUNA_ASSERT(m_gbuffer.albedo_view, "Failed to create GBuffer albedo view.");
    LUNA_ASSERT(m_gbuffer.normal_view, "Failed to create GBuffer normal view.");
    LUNA_ASSERT(m_gbuffer.material_view, "Failed to create GBuffer material view.");
    LUNA_ASSERT(m_gbuffer.depth_view, "Failed to create GBuffer depth view.");
    LUNA_ASSERT(m_gbuffer.final_color_view, "Failed to create GBuffer final color view.");
    LUNA_ASSERT(m_gbuffer.lighting_bind_group, "Failed to create GBuffer lighting bind group.");
    LUNA_CORE_DEBUG("GBuffer resized to {}x{}", width, height);

    m_frame_image = interface::FrameImage{
        .width = width,
        .height = height,
        .format = interface::FrameImageFormat::RGBA8_UNorm,
        .color_space = interface::FrameImageColorSpace::SRGB,
        .storage =
            interface::GpuFrameStorage{
                .texture = m_gbuffer.final_color_texture,
                .view = m_gbuffer.final_color_view,
            },
    };
}

void Renderer::beginFrame()
{
    ensureGBuffer(m_swapchain->getWidth(), m_swapchain->getHeight());
    LUNA_ASSERT(m_gbuffer.lighting_bind_group, "GBuffer is not initialized.");

    rhi::RenderPassBeginInfo pass;
    pass.color_attachments = {
        rhi::ColorAttachmentDesc{
            .view = m_gbuffer.albedo_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
        },
        rhi::ColorAttachmentDesc{
            .view = m_gbuffer.normal_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.5f, 0.5f, 1.0f, 1.0f},
        },
        rhi::ColorAttachmentDesc{
            .view = m_gbuffer.material_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
        },
    };
    pass.has_depth_stencil_attachment = true;
    pass.depth_stencil_attachment = rhi::DepthStencilAttachmentDesc{
        .view = m_gbuffer.depth_view,
        .depth_load_op = rhi::LoadOp::Clear,
        .depth_store_op = rhi::StoreOp::Store,
        .clear_depth = 1.0f,
    };
    pass.width = m_gbuffer.width;
    pass.height = m_gbuffer.height;

    m_cmd->begin();
    m_cmd->beginRenderPass(pass);
    m_cmd->setPipeline(m_geometry_pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);
}

void Renderer::endFrame()
{
    flushFrameUniforms();
    m_cmd->endRenderPass();

    const rhi::TextureBarrier barriers[] = {
        rhi::TextureBarrier{
            .texture = m_gbuffer.albedo_texture,
            .old_state = rhi::ResourceState::RenderTarget,
            .new_state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureBarrier{
            .texture = m_gbuffer.normal_texture,
            .old_state = rhi::ResourceState::RenderTarget,
            .new_state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureBarrier{
            .texture = m_gbuffer.material_texture,
            .old_state = rhi::ResourceState::RenderTarget,
            .new_state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureBarrier{
            .texture = m_gbuffer.depth_texture,
            .old_state = rhi::ResourceState::DepthStencilWrite,
            .new_state = rhi::ResourceState::ShaderRead,
        },
    };
    m_cmd->resourceBarrier(barriers, 4);

    rhi::RenderPassBeginInfo lightingPass;
    lightingPass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = m_gbuffer.final_color_view,
        .load_op = rhi::LoadOp::Clear,
        .store_op = rhi::StoreOp::Store,
        .clear_color = rhi::ClearColor{0.08f, 0.09f, 0.11f, 1.0f},
    });
    lightingPass.width = m_swapchain->getWidth();
    lightingPass.height = m_swapchain->getHeight();

    m_cmd->beginRenderPass(lightingPass);
    m_cmd->setPipeline(m_lighting_pipeline);
    m_cmd->setBindGroup(0, m_gbuffer.lighting_bind_group);
    m_cmd->draw(3);
    m_cmd->endRenderPass();

    const rhi::TextureBarrier finalColorBarrier{
        .texture = m_gbuffer.final_color_texture,
        .old_state = rhi::ResourceState::RenderTarget,
        .new_state = rhi::ResourceState::ShaderRead,
    };
    m_cmd->resourceBarrier(&finalColorBarrier, 1);

    m_cmd->end();
}

void Renderer::resize(uint32_t width, uint32_t height)
{
    ensureGBuffer(width, height);
}

void Renderer::setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos)
{
    m_frameUniforms.view = view;
    m_frameUniforms.projection = proj;
    m_frameUniforms.cameraPos = cameraPos;
    m_frame_uniforms_dirty = true;
}

void Renderer::setSceneLighting(const interface::SceneLighting& lighting)
{
    m_frameUniforms.directionalLightCount = lighting.directional_light_count > 0 ? 1u : 0u;
    m_frameUniforms.environmentAmbient = lighting.environment_ambient;

    if (m_frameUniforms.directionalLightCount > 0) {
        m_frameUniforms.lightDir = lighting.directional_light.direction;
        m_frameUniforms.lightAmbient = lighting.directional_light.ambient;
        m_frameUniforms.lightDiffuse = lighting.directional_light.diffuse;
        m_frameUniforms.lightSpecular = lighting.directional_light.specular;
    } else {
        m_frameUniforms.lightDir = {0.0f, -1.0f, 0.0f};
        m_frameUniforms.lightAmbient = {0.0f, 0.0f, 0.0f};
        m_frameUniforms.lightDiffuse = {0.0f, 0.0f, 0.0f};
        m_frameUniforms.lightSpecular = {0.0f, 0.0f, 0.0f};
    }

    m_frame_uniforms_dirty = true;
}

void Renderer::renderMesh(const interface::Mesh& mesh, const glm::mat4& transform)
{
    auto* gpu_mesh = getOrCreateMeshGpuData(mesh);
    if (gpu_mesh == nullptr || !gpu_mesh->vertex_buffer || gpu_mesh->vertex_count == 0) {
        return;
    }

    flushFrameUniforms();

    m_objectUniforms.model = transform;
    m_objectUniforms.normalMatrix = glm::transpose(glm::inverse(transform));
    m_device->updateBuffer(m_objectUniformBuffer, 0, &m_objectUniforms, sizeof(ObjectUniforms));

    m_cmd->setVertexBuffer(0, gpu_mesh->vertex_buffer);

    if (gpu_mesh->index_buffer && gpu_mesh->index_count > 0) {
        m_cmd->setIndexBuffer(gpu_mesh->index_buffer, rhi::IndexFormat::UInt32);
        m_cmd->drawIndexed(gpu_mesh->index_count);
    } else {
        m_cmd->draw(gpu_mesh->vertex_count);
    }
}


const interface::FrameImage& Renderer::getFrameImage() const
{
    return m_frame_image;
}

void Renderer::flushFrameUniforms()
{
    if (!m_frame_uniforms_dirty) {
        return;
    }

    m_device->updateBuffer(m_frameUniformBuffer, 0, &m_frameUniforms, sizeof(FrameUniforms));
    m_frame_uniforms_dirty = false;
}

Renderer::MeshGpuData* Renderer::getOrCreateMeshGpuData(const interface::Mesh& mesh)
{
    const auto& vertices = mesh.getVertices();
    const auto& indices = mesh.getIndices();
    if (vertices.empty()) {
        return nullptr;
    }

    LUNA_ASSERT(
        vertices.size() <= std::numeric_limits<uint32_t>::max(), "Mesh has too many vertices: {}", vertices.size());
    LUNA_ASSERT(
        indices.size() <= std::numeric_limits<uint32_t>::max(), "Mesh has too many indices: {}", indices.size());

    const auto key = getMeshCacheKey(mesh);
    auto& gpu_mesh = m_mesh_gpu_cache[key];

    const auto vertex_buffer_size = vertices.size() * sizeof(interface::Vertex);
    const auto index_buffer_size = indices.size() * sizeof(uint32_t);
    const auto vertex_version = mesh.getVertexVersion();
    const auto index_version = mesh.getIndexVersion();
    const auto vertex_changed =
        gpu_mesh.uploaded_vertex_version != 0 && gpu_mesh.uploaded_vertex_version != vertex_version;
    const auto index_changed = gpu_mesh.uploaded_index_version != 0 && gpu_mesh.uploaded_index_version != index_version;

    if (!gpu_mesh.vertex_buffer || gpu_mesh.vertex_buffer_capacity < vertex_buffer_size ||
        (vertex_changed && !gpu_mesh.vertex_buffer_dynamic)) {
        if (!gpu_mesh.vertex_buffer) {
            gpu_mesh.vertex_buffer_capacity = vertex_buffer_size;
            gpu_mesh.vertex_buffer_dynamic = false;
        } else {
            m_device->destroyBuffer(gpu_mesh.vertex_buffer);
            gpu_mesh.vertex_buffer_capacity = growBufferCapacity(gpu_mesh.vertex_buffer_capacity, vertex_buffer_size);
            gpu_mesh.vertex_buffer_dynamic = true;
        }

        gpu_mesh.vertex_buffer = m_device->createBuffer(
            rhi::BufferDesc{
                .size = gpu_mesh.vertex_buffer_capacity,
                .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
                .memory = gpu_mesh.vertex_buffer_dynamic ? rhi::MemoryUsage::CpuToGpu : rhi::MemoryUsage::GpuOnly,
            },
            nullptr);
        LUNA_ASSERT(gpu_mesh.vertex_buffer, "Failed to create mesh vertex buffer.");
        gpu_mesh.uploaded_vertex_version = 0;
    }

    if (gpu_mesh.uploaded_vertex_version != vertex_version) {
        m_device->updateBuffer(gpu_mesh.vertex_buffer, 0, vertices.data(), vertex_buffer_size);
        gpu_mesh.uploaded_vertex_version = vertex_version;
    }

    if (!indices.empty() && (!gpu_mesh.index_buffer || gpu_mesh.index_buffer_capacity < index_buffer_size ||
                             (index_changed && !gpu_mesh.index_buffer_dynamic))) {
        if (!gpu_mesh.index_buffer) {
            gpu_mesh.index_buffer_capacity = index_buffer_size;
            gpu_mesh.index_buffer_dynamic = false;
        } else {
            m_device->destroyBuffer(gpu_mesh.index_buffer);
            gpu_mesh.index_buffer_capacity = growBufferCapacity(gpu_mesh.index_buffer_capacity, index_buffer_size);
            gpu_mesh.index_buffer_dynamic = true;
        }

        gpu_mesh.index_buffer = m_device->createBuffer(
            rhi::BufferDesc{
                .size = gpu_mesh.index_buffer_capacity,
                .usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst,
                .memory = gpu_mesh.index_buffer_dynamic ? rhi::MemoryUsage::CpuToGpu : rhi::MemoryUsage::GpuOnly,
            },
            nullptr);
        LUNA_ASSERT(gpu_mesh.index_buffer, "Failed to create mesh index buffer.");
        gpu_mesh.uploaded_index_version = 0;
    }

    if (!indices.empty() && gpu_mesh.uploaded_index_version != index_version) {
        m_device->updateBuffer(gpu_mesh.index_buffer, 0, indices.data(), index_buffer_size);
        gpu_mesh.uploaded_index_version = index_version;
    } else if (indices.empty() && gpu_mesh.index_buffer) {
        m_device->destroyBuffer(gpu_mesh.index_buffer);
        gpu_mesh.index_buffer = {};
        gpu_mesh.index_buffer_dynamic = false;
        gpu_mesh.index_buffer_capacity = 0;
        gpu_mesh.uploaded_index_version = 0;
    }

    gpu_mesh.vertex_count = static_cast<uint32_t>(vertices.size());
    gpu_mesh.index_count = static_cast<uint32_t>(indices.size());
    return &gpu_mesh;
}

uint64_t Renderer::getMeshCacheKey(const interface::Mesh& mesh) const
{
    const auto handle = static_cast<uint64_t>(mesh.handle);
    LUNA_ASSERT(handle != 0);

    return handle;
}

} // namespace lunalite::renderer
