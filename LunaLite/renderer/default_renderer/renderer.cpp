#include "../../asset/asset_database.h"
#include "../../asset/builtin/builtin_assets.h"
#include "../../core/log.h"
#include "../interface/texture.h"
#include "renderer.h"

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

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

struct LineVertex {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
};

std::optional<std::string> readTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string loadDefaultRendererShader(const char* file_name)
{
    const auto relative_path =
        std::filesystem::path{"LunaLite"} / "renderer" / "default_renderer" / "shaders" / file_name;
#ifdef LUNALITE_SOURCE_ROOT
    const auto shader_path = std::filesystem::path{LUNALITE_SOURCE_ROOT} / relative_path;
#else
    const auto shader_path = relative_path;
#endif

    if (auto source = readTextFile(shader_path)) {
        return *source;
    }

    throw std::runtime_error("Failed to load default renderer shader '" + std::string{file_name} + "' from '" +
                             shader_path.string() + "'.");
}

uint32_t fullMipLevelCount(uint32_t width, uint32_t height)
{
    uint32_t levels = 1;
    uint32_t dimension = std::max(width, height);
    while (dimension > 1) {
        dimension /= 2;
        ++levels;
    }

    return levels;
}

rhi::TextureFormat toRHITextureFormat(const interface::TextureImportSettings& settings)
{
    return settings.color_space == interface::TextureColorSpace::SRGB ? rhi::TextureFormat::RGBA8_SRGB
                                                                      : rhi::TextureFormat::RGBA8_UNorm;
}

rhi::FilterMode toRHIFilter(interface::TextureFilter filter)
{
    switch (filter) {
        case interface::TextureFilter::Nearest:
            return rhi::FilterMode::Nearest;
        case interface::TextureFilter::Linear:
            return rhi::FilterMode::Linear;
    }

    return rhi::FilterMode::Linear;
}

rhi::MipFilter toRHIMipFilter(interface::TextureMipFilter filter)
{
    switch (filter) {
        case interface::TextureMipFilter::None:
            return rhi::MipFilter::None;
        case interface::TextureMipFilter::Nearest:
            return rhi::MipFilter::Nearest;
        case interface::TextureMipFilter::Linear:
            return rhi::MipFilter::Linear;
    }

    return rhi::MipFilter::Linear;
}

rhi::AddressMode toRHIAddressMode(interface::TextureAddressMode mode)
{
    switch (mode) {
        case interface::TextureAddressMode::Repeat:
            return rhi::AddressMode::Repeat;
        case interface::TextureAddressMode::ClampToEdge:
            return rhi::AddressMode::ClampToEdge;
        case interface::TextureAddressMode::MirroredRepeat:
            return rhi::AddressMode::MirroredRepeat;
    }

    return rhi::AddressMode::Repeat;
}

rhi::SamplerDesc toRHISamplerDesc(const interface::TextureSamplerDesc& sampler)
{
    return rhi::SamplerDesc{
        .min_filter = toRHIFilter(sampler.min_filter),
        .mag_filter = toRHIFilter(sampler.mag_filter),
        .mip_filter = toRHIMipFilter(sampler.mip_filter),
        .address_u = toRHIAddressMode(sampler.address_u),
        .address_v = toRHIAddressMode(sampler.address_v),
        .address_w = toRHIAddressMode(sampler.address_w),
    };
}

rhi::SamplerDesc fallbackSamplerDesc()
{
    return rhi::SamplerDesc{
        .min_filter = rhi::FilterMode::Linear,
        .mag_filter = rhi::FilterMode::Linear,
        .mip_filter = rhi::MipFilter::None,
        .address_u = rhi::AddressMode::ClampToEdge,
        .address_v = rhi::AddressMode::ClampToEdge,
        .address_w = rhi::AddressMode::ClampToEdge,
    };
}

rhi::BindGroupEntry combinedTextureEntry(uint32_t binding, rhi::TextureViewHandle view, rhi::SamplerHandle sampler)
{
    return rhi::BindGroupEntry{
        .binding = binding,
        .type = rhi::BindingType::CombinedImageSampler,
        .texture_view = view,
        .sampler = sampler,
    };
}

} // namespace

size_t Renderer::MaterialTextureKeyHash::operator()(const MaterialTextureKey& key) const noexcept
{
    size_t seed = 0;
    const auto combine = [&seed](asset::AssetHandle handle) {
        const auto value = std::hash<asset::AssetHandle>{}(handle);
        seed ^= value + 0x9E'37'79'B9u + (seed << 6) + (seed >> 2);
    };

    combine(key.albedo);
    combine(key.normal);
    combine(key.metallic_roughness);
    combine(key.occlusion);
    combine(key.emission);
    return seed;
}

Renderer::Renderer(rhi::Device& device, rhi::Swapchain& swapchain)
    : m_device(&device),
      m_swapchain(&swapchain)
{
    m_cmd = &m_device->getCommandList();

    const auto geometryVertexShaderSource = loadDefaultRendererShader("geometry.vert");
    const auto geometryFragmentShaderSource = loadDefaultRendererShader("geometry.frag");
    const auto lineVertexShaderSource = loadDefaultRendererShader("line.vert");
    const auto lineFragmentShaderSource = loadDefaultRendererShader("line.frag");
    const auto lightingVertexShaderSource = loadDefaultRendererShader("lighting.vert");
    const auto lightingFragmentShaderSource = loadDefaultRendererShader("lighting.frag");

    const auto geometryVertexShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = geometryVertexShaderSource.c_str(),
    });

    const auto geometryFragmentShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = geometryFragmentShaderSource.c_str(),
    });

    const auto lineVertexShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = lineVertexShaderSource.c_str(),
    });

    const auto lineFragmentShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = lineFragmentShaderSource.c_str(),
    });

    const auto lightingVertexShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = lightingVertexShaderSource.c_str(),
    });

    const auto lightingFragmentShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = lightingFragmentShaderSource.c_str(),
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

    m_geometry_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_geometry_bind_group_layout, m_material_texture_bind_group_layout},
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
                                                                  rhi::VertexAttributeDesc{
                                                                      .location = 2,
                                                                      .format = rhi::VertexFormat::Float2,
                                                                      .offset = offsetof(interface::Vertex, uv),
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
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA16F},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
                                                      rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA16F},
                                                  },
                                              .has_depth_stencil = true,
                                              .depth_stencil_format = rhi::TextureFormat::Depth24Stencil8,
                                          },
                                  });

    m_line_pipeline = m_device
                          ->createPipeline(
                              rhi::PipelineDesc{
                                  .topology = rhi::PrimitiveTopology::Line,
                                  .vertex_input =
                                      rhi::VertexInputDesc{
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
                                      },
                                  .layout = m_geometry_pipeline_layout,
                                  .vertex_shader = lineVertexShader,
                                  .fragment_shader = lineFragmentShader,
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
                        rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
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

    m_line_vertex_buffer = m_device->createBuffer(
        rhi::BufferDesc{
            .size = sizeof(LineVertex) * 2,
            .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
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
    LUNA_ASSERT(lineVertexShader, "Failed to create line vertex shader.");
    LUNA_ASSERT(lineFragmentShader, "Failed to create line fragment shader.");
    LUNA_ASSERT(lightingVertexShader, "Failed to create lighting vertex shader.");
    LUNA_ASSERT(lightingFragmentShader, "Failed to create lighting fragment shader.");
    LUNA_ASSERT(m_geometry_bind_group_layout, "Failed to create geometry bind group layout.");
    LUNA_ASSERT(m_material_texture_bind_group_layout, "Failed to create material texture bind group layout.");
    LUNA_ASSERT(m_lighting_bind_group_layout, "Failed to create lighting bind group layout.");
    LUNA_ASSERT(m_geometry_pipeline_layout, "Failed to create geometry pipeline layout.");
    LUNA_ASSERT(m_lighting_pipeline_layout, "Failed to create lighting pipeline layout.");
    LUNA_ASSERT(m_geometry_pipeline, "Failed to create geometry pipeline.");
    LUNA_ASSERT(m_lighting_pipeline, "Failed to create lighting pipeline.");
    LUNA_ASSERT(m_line_pipeline, "Failed to create line pipeline.");
    LUNA_ASSERT(m_frameUniformBuffer, "Failed to create frame uniform buffer.");
    LUNA_ASSERT(m_objectUniformBuffer, "Failed to create object uniform buffer.");
    LUNA_ASSERT(m_line_vertex_buffer, "Failed to create line vertex buffer.");
    LUNA_ASSERT(m_geometry_bind_group, "Failed to create geometry bind group.");
    LUNA_ASSERT(m_gbuffer_sampler, "Failed to create GBuffer sampler.");
    LUNA_CORE_DEBUG("Default renderer initialized");
}

Renderer::~Renderer()
{
    destroyMaterialTextureResources();
}

const Renderer::TextureGpuResource& Renderer::getOrCreateTextureGpuResource(asset::AssetHandle handle,
                                                                            FallbackTexture fallback)
{
    if (!handle.isValid()) {
        return getOrCreateFallbackTexture(fallback);
    }

    if (const auto it = m_texture_gpu_cache.find(handle); it != m_texture_gpu_cache.end()) {
        return it->second;
    }

    const auto* texture = asset::AssetDatabase::get().get<interface::Texture>(handle);
    if (texture == nullptr || texture->getWidth() == 0 || texture->getHeight() == 0 || texture->getPixels().empty()) {
        return getOrCreateFallbackTexture(fallback);
    }

    const auto& settings = texture->getImportSettings();
    const auto rhiFormat = toRHITextureFormat(settings);
    const auto mipLevels = settings.generate_mipmaps ? fullMipLevelCount(texture->getWidth(), texture->getHeight()) : 1;

    TextureGpuResource resource;
    resource.texture = m_device->createTexture(rhi::TextureDesc{
        .width = texture->getWidth(),
        .height = texture->getHeight(),
        .format = rhiFormat,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst,
        .mip_levels = mipLevels,
    });
    if (!resource.texture) {
        return getOrCreateFallbackTexture(fallback);
    }

    m_device->updateTexture(resource.texture,
                            rhi::TextureUploadDesc{
                                .width = texture->getWidth(),
                                .height = texture->getHeight(),
                                .format = rhiFormat,
                                .data = texture->getPixels().data(),
                                .row_pitch = static_cast<size_t>(texture->getWidth()) * 4,
                            });

    if (mipLevels > 1) {
        m_device->generateMipmaps(resource.texture);
    }

    resource.view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = resource.texture,
        .format = rhiFormat,
        .aspect = rhi::TextureAspect::Color,
        .mip_level_count = mipLevels,
    });

    auto samplerDesc = toRHISamplerDesc(settings.sampler);
    if (mipLevels == 1) {
        samplerDesc.mip_filter = rhi::MipFilter::None;
    }
    resource.sampler = m_device->createSampler(samplerDesc);

    if (!resource.view || !resource.sampler) {
        destroyTextureGpuResource(resource);
        return getOrCreateFallbackTexture(fallback);
    }

    return m_texture_gpu_cache.emplace(handle, resource).first->second;
}

const Renderer::TextureGpuResource& Renderer::getOrCreateFallbackTexture(FallbackTexture fallback)
{
    auto& resource =
        fallback == FallbackTexture::FlatNormal ? m_flat_normal_fallback_texture : m_white_fallback_texture;
    if (resource.texture && resource.view && resource.sampler) {
        return resource;
    }

    const std::array<uint8_t, 4> pixel = fallback == FallbackTexture::FlatNormal
                                             ? std::array<uint8_t, 4>{128, 128, 255, 255}
                                             : std::array<uint8_t, 4>{255, 255, 255, 255};

    resource.texture = m_device->createTexture(rhi::TextureDesc{
        .width = 1,
        .height = 1,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst,
    });
    resource.view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = resource.texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });
    resource.sampler = m_device->createSampler(fallbackSamplerDesc());

    if (resource.texture) {
        m_device->updateTexture(resource.texture,
                                rhi::TextureUploadDesc{
                                    .width = 1,
                                    .height = 1,
                                    .format = rhi::TextureFormat::RGBA8_UNorm,
                                    .data = pixel.data(),
                                    .row_pitch = pixel.size(),
                                });
    }

    return resource;
}

const Renderer::MaterialGpuResource&
    Renderer::getOrCreateMaterialGpuResource(const interface::MaterialParameters& parameters)
{
    const MaterialTextureKey key{
        .albedo = parameters.albedo_texture,
        .normal = parameters.normal_texture,
        .metallic_roughness = parameters.metallic_roughness_texture,
        .occlusion = parameters.occlusion_texture,
        .emission = parameters.emission_texture,
    };

    if (const auto it = m_material_gpu_cache.find(key); it != m_material_gpu_cache.end()) {
        return it->second;
    }

    const auto& albedo = getOrCreateTextureGpuResource(key.albedo, FallbackTexture::White);
    const auto& normal = getOrCreateTextureGpuResource(key.normal, FallbackTexture::FlatNormal);
    const auto& metallicRoughness = getOrCreateTextureGpuResource(key.metallic_roughness, FallbackTexture::White);
    const auto& occlusion = getOrCreateTextureGpuResource(key.occlusion, FallbackTexture::White);
    const auto& emission = getOrCreateTextureGpuResource(key.emission, FallbackTexture::White);

    const auto bindGroup = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_material_texture_bind_group_layout,
        .entries =
            {
                combinedTextureEntry(0, albedo.view, albedo.sampler),
                combinedTextureEntry(1, normal.view, normal.sampler),
                combinedTextureEntry(2, metallicRoughness.view, metallicRoughness.sampler),
                combinedTextureEntry(3, occlusion.view, occlusion.sampler),
                combinedTextureEntry(4, emission.view, emission.sampler),
            },
    });

    return m_material_gpu_cache.emplace(key, MaterialGpuResource{.bind_group = bindGroup}).first->second;
}

void Renderer::destroyTextureGpuResource(TextureGpuResource& resource)
{
    if (resource.sampler) {
        m_device->destroySampler(resource.sampler);
    }
    if (resource.view) {
        m_device->destroyTextureView(resource.view);
    }
    if (resource.texture) {
        m_device->destroyTexture(resource.texture);
    }
    resource = {};
}

void Renderer::destroyMaterialTextureResources()
{
    for (const auto& [_, resource] : m_material_gpu_cache) {
        if (resource.bind_group) {
            m_device->destroyBindGroup(resource.bind_group);
        }
    }
    m_material_gpu_cache.clear();

    for (auto& [_, resource] : m_texture_gpu_cache) {
        destroyTextureGpuResource(resource);
    }
    m_texture_gpu_cache.clear();

    destroyTextureGpuResource(m_white_fallback_texture);
    destroyTextureGpuResource(m_flat_normal_fallback_texture);
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
        m_gbuffer.emission_view,
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
        m_gbuffer.emission_texture,
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
        .format = rhi::TextureFormat::RGBA8_UNorm,
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
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
    });
    m_gbuffer.emission_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA16F,
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
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
    });

    m_gbuffer.albedo_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.albedo_texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.normal_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.normal_texture,
        .format = rhi::TextureFormat::RGBA16F,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.material_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.material_texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.emission_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.emission_texture,
        .format = rhi::TextureFormat::RGBA16F,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.depth_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.depth_texture,
        .format = rhi::TextureFormat::Depth24Stencil8,
        .aspect = rhi::TextureAspect::DepthStencil,
    });
    m_gbuffer.final_color_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.final_color_texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
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
                                                        rhi::BindGroupEntry{
                                                            .binding = 5,
                                                            .type = rhi::BindingType::CombinedImageSampler,
                                                            .texture_view = m_gbuffer.emission_view,
                                                            .sampler = m_gbuffer_sampler,
                                                        },
                                                    },
                                            });

    LUNA_ASSERT(m_gbuffer.albedo_texture, "Failed to create GBuffer albedo texture.");
    LUNA_ASSERT(m_gbuffer.normal_texture, "Failed to create GBuffer normal texture.");
    LUNA_ASSERT(m_gbuffer.material_texture, "Failed to create GBuffer material texture.");
    LUNA_ASSERT(m_gbuffer.emission_texture, "Failed to create GBuffer emission texture.");
    LUNA_ASSERT(m_gbuffer.depth_texture, "Failed to create GBuffer depth texture.");
    LUNA_ASSERT(m_gbuffer.final_color_texture, "Failed to create GBuffer final color texture.");
    LUNA_ASSERT(m_gbuffer.albedo_view, "Failed to create GBuffer albedo view.");
    LUNA_ASSERT(m_gbuffer.normal_view, "Failed to create GBuffer normal view.");
    LUNA_ASSERT(m_gbuffer.material_view, "Failed to create GBuffer material view.");
    LUNA_ASSERT(m_gbuffer.emission_view, "Failed to create GBuffer emission view.");
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
        rhi::ColorAttachmentDesc{
            .view = m_gbuffer.emission_view,
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
            .texture = m_gbuffer.emission_texture,
            .old_state = rhi::ResourceState::RenderTarget,
            .new_state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureBarrier{
            .texture = m_gbuffer.depth_texture,
            .old_state = rhi::ResourceState::DepthStencilWrite,
            .new_state = rhi::ResourceState::ShaderRead,
        },
    };
    m_cmd->resourceBarrier(barriers, 5);

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

void Renderer::renderModel(const interface::Model& model, const glm::mat4& transform)
{
    const interface::Material* fallbackMaterial = nullptr;
    bool fallbackMaterialResolved = false;
    const auto getFallbackMaterial = [&]() {
        if (!fallbackMaterialResolved) {
            fallbackMaterial =
                asset::AssetDatabase::get().get<interface::Material>(asset::builtin::errorMaterialHandle());
            fallbackMaterialResolved = true;
        }
        return fallbackMaterial;
    };

    for (const auto& modelMesh : model.getMeshes()) {
        if (!modelMesh.mesh.isValid()) {
            continue;
        }

        const auto* mesh = asset::AssetDatabase::get().get<interface::Mesh>(modelMesh.mesh);
        if (mesh == nullptr) {
            LUNA_CORE_ERROR("Model {} has a null mesh asset", static_cast<uint64_t>(model.handle));
            continue;
        }

        const auto& submeshes = mesh->getSubMeshes();
        for (size_t submeshIndex = 0; submeshIndex < submeshes.size(); ++submeshIndex) {
            const auto& submesh = submeshes[submeshIndex];
            const interface::Material* material = nullptr;
            if (submesh.material_slot < modelMesh.materials.size()) {
                const auto materialHandle = modelMesh.materials[submesh.material_slot];
                if (materialHandle.isValid()) {
                    if (const auto* resolvedMaterial =
                            asset::AssetDatabase::get().get<interface::Material>(materialHandle)) {
                        material = resolvedMaterial;
                    }
                }
            }

            if (material == nullptr) {
                material = getFallbackMaterial();
            }

            if (material != nullptr) {
                drawSubMesh(*mesh, submeshIndex, submesh, *material, transform);
            }
        }
    }
}

void Renderer::drawSubMesh(const interface::Mesh& mesh,
                           size_t submesh_index,
                           const interface::SubMesh& submesh,
                           const interface::Material& material,
                           const glm::mat4& transform)
{
    auto* gpu_mesh = getOrCreateSubMeshGpuData(mesh, submesh_index, submesh);
    if (gpu_mesh == nullptr || !gpu_mesh->vertex_buffer || gpu_mesh->vertex_count == 0) {
        return;
    }

    flushFrameUniforms();

    const interface::MaterialParameters defaultParameters;
    const auto& parameters = material.parameters != nullptr ? *material.parameters : defaultParameters;
    const auto& materialGpu = getOrCreateMaterialGpuResource(parameters);
    m_objectUniforms.model = transform;
    m_objectUniforms.normalMatrix = glm::transpose(glm::inverse(transform));
    m_objectUniforms.materialAlbedo = parameters.albedo;
    m_objectUniforms.materialEmission = parameters.emission;
    m_objectUniforms.materialEmissionStrength = parameters.emission_strength;
    m_objectUniforms.materialMetallic = parameters.metallic;
    m_objectUniforms.materialRoughness = parameters.roughness;
    m_objectUniforms.materialShadingModel = parameters.shading_model == interface::ShadingModel::Unlit ? 1u : 0u;
    m_objectUniforms.materialNormalScale = parameters.normal_scale;
    m_objectUniforms.materialOcclusionStrength = parameters.occlusion_strength;
    m_device->updateBuffer(m_objectUniformBuffer, 0, &m_objectUniforms, sizeof(ObjectUniforms));

    m_cmd->setVertexBuffer(0, gpu_mesh->vertex_buffer);
    if (materialGpu.bind_group) {
        m_cmd->setBindGroup(1, materialGpu.bind_group);
    }

    if (gpu_mesh->index_buffer && gpu_mesh->index_count > 0) {
        m_cmd->setIndexBuffer(gpu_mesh->index_buffer, rhi::IndexFormat::UInt32);
        m_cmd->drawIndexed(gpu_mesh->index_count);
    } else {
        m_cmd->draw(gpu_mesh->vertex_count);
    }
}

void Renderer::renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    const LineVertex vertices[] = {
        LineVertex{.position = start, .color = color},
        LineVertex{.position = end, .color = color},
    };

    flushFrameUniforms();

    m_objectUniforms.model = glm::mat4{1.0f};
    m_objectUniforms.normalMatrix = glm::mat4{1.0f};
    m_device->updateBuffer(m_objectUniformBuffer, 0, &m_objectUniforms, sizeof(ObjectUniforms));
    m_device->updateBuffer(m_line_vertex_buffer, 0, vertices, sizeof(vertices));

    m_cmd->setPipeline(m_line_pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);
    m_cmd->setVertexBuffer(0, m_line_vertex_buffer);
    m_cmd->draw(2);
    m_cmd->setPipeline(m_geometry_pipeline);
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

Renderer::MeshGpuData* Renderer::getOrCreateSubMeshGpuData(const interface::Mesh& mesh,
                                                           size_t submesh_index,
                                                           const interface::SubMesh& submesh)
{
    const auto& vertices = submesh.getVertices();
    const auto& indices = submesh.getIndices();
    if (vertices.empty()) {
        return nullptr;
    }

    LUNA_ASSERT(
        vertices.size() <= std::numeric_limits<uint32_t>::max(), "Mesh has too many vertices: {}", vertices.size());
    LUNA_ASSERT(
        indices.size() <= std::numeric_limits<uint32_t>::max(), "Mesh has too many indices: {}", indices.size());

    const auto key = getSubMeshCacheKey(mesh, submesh_index);
    auto& gpu_mesh = m_mesh_gpu_cache[key];

    const auto vertex_buffer_size = vertices.size() * sizeof(interface::Vertex);
    const auto index_buffer_size = indices.size() * sizeof(uint32_t);
    const auto vertex_version = submesh.getVertexVersion();
    const auto index_version = submesh.getIndexVersion();
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

uint64_t Renderer::getSubMeshCacheKey(const interface::Mesh& mesh, size_t submesh_index) const
{
    const auto handle = static_cast<uint64_t>(mesh.handle);
    LUNA_ASSERT(handle != 0);

    return handle ^ ((static_cast<uint64_t>(submesh_index) + 1u) * 0x9E'37'79'B9'7F'4A'7C'15ull);
}

} // namespace lunalite::renderer
