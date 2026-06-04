#include "../../asset/asset_database.h"
#include "../../asset/asset_manager.h"
#include "../../asset/builtin/builtin_assets.h"
#include "../../core/log.h"
#include "../interface/texture.h"
#include "../rhi_upload_helpers.h"
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
#include <utility>
#include <vector>

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

rhi::TextureFormat toRHITextureFormat(interface::TextureFormat format, const interface::TextureImportSettings& settings)
{
    switch (format) {
        case interface::TextureFormat::RGBA32F:
            return rhi::TextureFormat::RGBA32F;
        case interface::TextureFormat::RGBA8_UNorm:
        default:
            return settings.color_space == interface::TextureColorSpace::SRGB ? rhi::TextureFormat::RGBA8_SRGB
                                                                              : rhi::TextureFormat::RGBA8_UNorm;
    }
}

size_t textureBytesPerPixel(interface::TextureFormat format)
{
    switch (format) {
        case interface::TextureFormat::RGBA32F:
            return sizeof(float) * 4;
        case interface::TextureFormat::RGBA8_UNorm:
        default:
            return 4;
    }
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

struct EnvironmentSettings {
    uint32_t irradiance_size{32};
    uint32_t prefilter_size{128};
};

std::optional<EnvironmentSettings> readEnvironmentSettings(asset::AssetHandle handle)
{
    if (!handle.isValid()) {
        return std::nullopt;
    }

    const auto* metadata = asset::AssetManager::get().getMetadata(handle);
    if (metadata == nullptr) {
        return std::nullopt;
    }

    const auto environment = metadata->SpecializedConfig["Environment"];
    if (!environment || environment["Type"].as<std::string>("") != "EquirectangularHDR") {
        return std::nullopt;
    }

    EnvironmentSettings settings;
    settings.irradiance_size = std::max(environment["IrradianceSize"].as<uint32_t>(settings.irradiance_size), 1u);
    settings.prefilter_size = std::max(environment["PrefilterSize"].as<uint32_t>(settings.prefilter_size), 1u);
    return settings;
}

rhi::TextureHandle createCubeTexture(rhi::Device& device,
                                     uint32_t size,
                                     uint32_t mipCount,
                                     rhi::TextureFormat format,
                                     rhi::TextureUsage usage,
                                     rhi::ResourceState initialState = rhi::ResourceState::CopyDst)
{
    auto texture = device.createTexture(rhi::TextureDesc{
        .width = size,
        .height = size,
        .dimension = rhi::TextureDimension::TextureCube,
        .format = format,
        .usage = usage,
        .mip_levels = mipCount,
        .array_layers = 6,
        .initial_state = initialState,
    });
    return texture;
}

rhi::TextureViewHandle
    createCubeTextureView(rhi::Device& device, rhi::TextureHandle texture, rhi::TextureFormat format, uint32_t mipCount)
{
    return device.createTextureView(rhi::TextureViewDesc{
        .texture = texture,
        .view_dimension = rhi::TextureViewDimension::TextureCube,
        .format = format,
        .aspect = rhi::TextureAspect::Color,
        .mip_level_count = mipCount,
        .array_layer_count = 6,
    });
}

rhi::TextureViewHandle createCubeFaceView(
    rhi::Device& device, rhi::TextureHandle texture, rhi::TextureFormat format, uint32_t mipLevel, uint32_t face)
{
    return device.createTextureView(rhi::TextureViewDesc{
        .texture = texture,
        .view_dimension = rhi::TextureViewDimension::Texture2D,
        .format = format,
        .aspect = rhi::TextureAspect::Color,
        .base_mip_level = mipLevel,
        .mip_level_count = 1,
        .base_array_layer = face,
        .array_layer_count = 1,
    });
}

rhi::SamplerHandle createCubeSampler(rhi::Device& device, uint32_t mipCount)
{
    return device.createSampler(rhi::SamplerDesc{
        .min_filter = rhi::FilterMode::Linear,
        .mag_filter = rhi::FilterMode::Linear,
        .mip_filter = mipCount > 1 ? rhi::MipFilter::Linear : rhi::MipFilter::None,
        .address_u = rhi::AddressMode::ClampToEdge,
        .address_v = rhi::AddressMode::ClampToEdge,
        .address_w = rhi::AddressMode::ClampToEdge,
    });
}

uint32_t mipDimension(uint32_t base, uint32_t mip)
{
    uint32_t value = base;
    for (uint32_t i = 0; i < mip && value > 1; ++i) {
        value /= 2;
    }

    return std::max(1u, value);
}

uint32_t lastMipLevel(uint32_t mipLevels)
{
    return mipLevels > 0 ? mipLevels - 1 : 0;
}

uint32_t prefilterSampleCount(uint32_t mip, uint32_t maxMip)
{
    if (maxMip == 0) {
        return 1'024;
    }

    const float roughness = static_cast<float>(mip) / static_cast<float>(maxMip);
    if (roughness >= 0.75f) {
        return 8'192;
    }
    if (roughness >= 0.5f) {
        return 4'096;
    }
    if (roughness >= 0.25f) {
        return 2'048;
    }
    return 1'024;
}

constexpr uint32_t DefaultEnvironmentCubeSize = 512;
constexpr uint32_t DefaultIrradianceCubeSize = 32;
constexpr uint32_t DefaultPrefilterCubeSize = 128;
constexpr uint32_t EnvironmentComputeGroupSize = 8;
constexpr uint32_t BrdfLutSize = 512;
constexpr uint32_t BrdfLutComputeGroupSize = 8;
constexpr rhi::TextureFormat EnvironmentCubeFormat = rhi::TextureFormat::RGBA32F;
constexpr rhi::TextureFormat FilteredEnvironmentCubeFormat = rhi::TextureFormat::RGBA16F;

constexpr const char* BrdfLutComputeShader = R"GLSL(
#version 450 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rg16f, binding = 0) writeonly uniform image2D uBRDFLut;

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 1024u;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint n)
{
    return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

vec3 ImportanceSampleGGX(vec2 xi, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float a = roughness;
    float k = (a * a) * 0.5;
    float denom = nDotV * (1.0 - k) + k;
    return nDotV / denom;
}

float GeometrySmith(float nDotV, float nDotL, float roughness)
{
    return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
}

vec2 IntegrateBRDF(float nDotV, float roughness)
{
    vec3 view = vec3(sqrt(max(1.0 - nDotV * nDotV, 0.0)), 0.0, nDotV);

    float scale = 0.0;
    float bias = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 xi = Hammersley(i, SAMPLE_COUNT);
        vec3 halfVector = ImportanceSampleGGX(xi, roughness);
        vec3 light = normalize(2.0 * dot(view, halfVector) * halfVector - view);

        float nDotL = max(light.z, 0.0);
        float nDotH = max(halfVector.z, 0.0);
        float vDotH = max(dot(view, halfVector), 0.0);

        if (nDotL > 0.0) {
            float geometry = GeometrySmith(nDotV, nDotL, roughness);
            float geometryVisibility = (geometry * vDotH) / max(nDotH * nDotV, 0.001);
            float fresnel = pow(1.0 - vDotH, 5.0);

            scale += (1.0 - fresnel) * geometryVisibility;
            bias += fresnel * geometryVisibility;
        }
    }

    return vec2(scale, bias) / float(SAMPLE_COUNT);
}

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uBRDFLut);
    if (pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(size);
    float nDotV = max(uv.x, 0.001);
    float roughness = max(uv.y, 0.001);
    vec2 integrated = IntegrateBRDF(nDotV, roughness);

    imageStore(uBRDFLut, pixel, vec4(integrated, 0.0, 1.0));
}
)GLSL";

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

    const auto geometryVertexShaderSource = loadDefaultRendererShader("geometry.vert");
    const auto geometryFragmentShaderSource = loadDefaultRendererShader("geometry.frag");
    const auto lineVertexShaderSource = loadDefaultRendererShader("line.vert");
    const auto lineFragmentShaderSource = loadDefaultRendererShader("line.frag");
    const auto lightingVertexShaderSource = loadDefaultRendererShader("lighting.vert");
    const auto lightingFragmentShaderSource = loadDefaultRendererShader("lighting.frag");
    const auto skyboxVertexShaderSource = loadDefaultRendererShader("skybox.vert");
    const auto skyboxFragmentShaderSource = loadDefaultRendererShader("skybox.frag");
    const auto environmentCubemapComputeShaderSource = loadDefaultRendererShader("environment_cubemap.comp");
    const auto environmentIrradianceComputeShaderSource = loadDefaultRendererShader("environment_irradiance.comp");
    const auto environmentPrefilterComputeShaderSource = loadDefaultRendererShader("environment_prefilter.comp");

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

    const auto skyboxVertexShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = skyboxVertexShaderSource.c_str(),
    });

    const auto skyboxFragmentShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = skyboxFragmentShaderSource.c_str(),
    });

    const auto environmentCubemapComputeShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = environmentCubemapComputeShaderSource.c_str(),
    });

    const auto environmentIrradianceComputeShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = environmentIrradianceComputeShaderSource.c_str(),
    });

    const auto environmentPrefilterComputeShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = environmentPrefilterComputeShaderSource.c_str(),
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

    m_geometry_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_geometry_bind_group_layout, m_material_texture_bind_group_layout},
    });

    m_lighting_pipeline_layout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_lighting_bind_group_layout, m_environment_bind_group_layout},
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

    m_skybox_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_input = rhi::VertexInputDesc{},
        .layout = m_skybox_pipeline_layout,
        .vertex_shader = skyboxVertexShader,
        .fragment_shader = skyboxFragmentShader,
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

    m_environment_cubemap_pipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = m_environment_compute_pipeline_layout,
        .compute_shader = environmentCubemapComputeShader,
    });

    m_environment_irradiance_pipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = m_environment_compute_pipeline_layout,
        .compute_shader = environmentIrradianceComputeShader,
    });

    m_environment_prefilter_pipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = m_environment_compute_pipeline_layout,
        .compute_shader = environmentPrefilterComputeShader,
    });

    m_frameUniformBuffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(FrameUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::UniformRead,
    });

    m_objectUniformBuffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(ObjectUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::UniformRead,
    });

    m_line_vertex_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(LineVertex) * 2,
        .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::VertexBuffer,
    });

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
    createBlackEnvironmentGpuResource();
    createBrdfLutResource();
    m_environment_bind_group = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_environment_bind_group_layout,
        .entries =
            {
                combinedTextureEntry(0,
                                     m_black_environment_gpu_resource.environment.view,
                                     m_black_environment_gpu_resource.environment.sampler),
                combinedTextureEntry(1,
                                     m_black_environment_gpu_resource.irradiance.view,
                                     m_black_environment_gpu_resource.irradiance.sampler),
                combinedTextureEntry(2,
                                     m_black_environment_gpu_resource.prefilter.view,
                                     m_black_environment_gpu_resource.prefilter.sampler),
                combinedTextureEntry(3, m_brdf_lut_resource.view, m_brdf_lut_resource.sampler),
            },
    });

    LUNA_ASSERT(geometryVertexShader, "Failed to create geometry vertex shader.");
    LUNA_ASSERT(geometryFragmentShader, "Failed to create geometry fragment shader.");
    LUNA_ASSERT(lineVertexShader, "Failed to create line vertex shader.");
    LUNA_ASSERT(lineFragmentShader, "Failed to create line fragment shader.");
    LUNA_ASSERT(lightingVertexShader, "Failed to create lighting vertex shader.");
    LUNA_ASSERT(lightingFragmentShader, "Failed to create lighting fragment shader.");
    LUNA_ASSERT(skyboxVertexShader, "Failed to create skybox vertex shader.");
    LUNA_ASSERT(skyboxFragmentShader, "Failed to create skybox fragment shader.");
    LUNA_ASSERT(environmentCubemapComputeShader, "Failed to create environment cubemap compute shader.");
    LUNA_ASSERT(environmentIrradianceComputeShader, "Failed to create environment irradiance compute shader.");
    LUNA_ASSERT(environmentPrefilterComputeShader, "Failed to create environment prefilter compute shader.");
    LUNA_ASSERT(m_geometry_bind_group_layout, "Failed to create geometry bind group layout.");
    LUNA_ASSERT(m_material_texture_bind_group_layout, "Failed to create material texture bind group layout.");
    LUNA_ASSERT(m_lighting_bind_group_layout, "Failed to create lighting bind group layout.");
    LUNA_ASSERT(m_environment_bind_group_layout, "Failed to create environment bind group layout.");
    LUNA_ASSERT(m_environment_compute_bind_group_layout, "Failed to create environment compute bind group layout.");
    LUNA_ASSERT(m_geometry_pipeline_layout, "Failed to create geometry pipeline layout.");
    LUNA_ASSERT(m_lighting_pipeline_layout, "Failed to create lighting pipeline layout.");
    LUNA_ASSERT(m_skybox_pipeline_layout, "Failed to create skybox pipeline layout.");
    LUNA_ASSERT(m_environment_compute_pipeline_layout, "Failed to create environment compute pipeline layout.");
    LUNA_ASSERT(m_geometry_pipeline, "Failed to create geometry pipeline.");
    LUNA_ASSERT(m_lighting_pipeline, "Failed to create lighting pipeline.");
    LUNA_ASSERT(m_skybox_pipeline, "Failed to create skybox pipeline.");
    LUNA_ASSERT(m_line_pipeline, "Failed to create line pipeline.");
    LUNA_ASSERT(m_environment_cubemap_pipeline, "Failed to create environment cubemap pipeline.");
    LUNA_ASSERT(m_environment_irradiance_pipeline, "Failed to create environment irradiance pipeline.");
    LUNA_ASSERT(m_environment_prefilter_pipeline, "Failed to create environment prefilter pipeline.");
    LUNA_ASSERT(m_frameUniformBuffer, "Failed to create frame uniform buffer.");
    LUNA_ASSERT(m_objectUniformBuffer, "Failed to create object uniform buffer.");
    LUNA_ASSERT(m_line_vertex_buffer, "Failed to create line vertex buffer.");
    LUNA_ASSERT(m_geometry_bind_group, "Failed to create geometry bind group.");
    LUNA_ASSERT(m_black_environment_gpu_resource.environment.texture, "Failed to create black environment texture.");
    LUNA_ASSERT(m_black_environment_gpu_resource.environment.view, "Failed to create black environment view.");
    LUNA_ASSERT(m_black_environment_gpu_resource.environment.sampler, "Failed to create black environment sampler.");
    LUNA_ASSERT(m_black_environment_gpu_resource.irradiance.texture, "Failed to create black irradiance texture.");
    LUNA_ASSERT(m_black_environment_gpu_resource.irradiance.view, "Failed to create black irradiance view.");
    LUNA_ASSERT(m_black_environment_gpu_resource.irradiance.sampler, "Failed to create black irradiance sampler.");
    LUNA_ASSERT(m_black_environment_gpu_resource.prefilter.texture, "Failed to create black prefilter texture.");
    LUNA_ASSERT(m_black_environment_gpu_resource.prefilter.view, "Failed to create black prefilter view.");
    LUNA_ASSERT(m_black_environment_gpu_resource.prefilter.sampler, "Failed to create black prefilter sampler.");
    LUNA_ASSERT(m_brdf_lut_resource.texture, "Failed to create BRDF LUT texture.");
    LUNA_ASSERT(m_brdf_lut_resource.view, "Failed to create BRDF LUT view.");
    LUNA_ASSERT(m_brdf_lut_resource.sampler, "Failed to create BRDF LUT sampler.");
    LUNA_ASSERT(m_environment_bind_group, "Failed to create environment bind group.");
    LUNA_ASSERT(m_gbuffer_sampler, "Failed to create GBuffer sampler.");
    LUNA_CORE_DEBUG("Default renderer initialized");
}

Renderer::~Renderer()
{
    if (m_environment_bind_group) {
        m_device->destroyBindGroup(m_environment_bind_group);
        m_environment_bind_group = {};
    }
    destroyMaterialTextureResources();
    destroyTextureGpuResource(m_brdf_lut_resource);
    destroyEnvironmentGpuResource(m_black_environment_gpu_resource);
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
    const auto rhiFormat = toRHITextureFormat(texture->getFormat(), settings);
    const auto mipLevels = settings.generate_mipmaps ? fullMipLevelCount(texture->getWidth(), texture->getHeight()) : 1;

    TextureGpuResource resource;
    resource.texture = m_device->createTexture(rhi::TextureDesc{
        .width = texture->getWidth(),
        .height = texture->getHeight(),
        .format = rhiFormat,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst |
                 (mipLevels > 1 ? rhi::TextureUsage::CopySrc : rhi::TextureUsage::None),
        .mip_levels = mipLevels,
        .initial_state = rhi::ResourceState::CopyDst,
    });
    if (!resource.texture) {
        return getOrCreateFallbackTexture(fallback);
    }

    if (!rhi_upload::uploadTextureData(
            *m_device,
            m_upload_command_list,
            resource.texture,
            rhi_upload::TextureUploadData{
                .data = texture->getPixels().data(),
                .size = static_cast<size_t>(texture->getHeight()) * texture->getWidth() *
                        textureBytesPerPixel(texture->getFormat()),
                .row_pitch = static_cast<size_t>(texture->getWidth()) * textureBytesPerPixel(texture->getFormat()),
                .width = texture->getWidth(),
                .height = texture->getHeight(),
            },
            mipLevels > 1)) {
        destroyTextureGpuResource(resource);
        return getOrCreateFallbackTexture(fallback);
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
        .initial_state = rhi::ResourceState::CopyDst,
    });
    resource.view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = resource.texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });
    resource.sampler = m_device->createSampler(fallbackSamplerDesc());

    if (resource.texture) {
        rhi_upload::uploadTextureData(*m_device,
                                      m_upload_command_list,
                                      resource.texture,
                                      rhi_upload::TextureUploadData{
                                          .data = pixel.data(),
                                          .size = pixel.size(),
                                          .row_pitch = pixel.size(),
                                          .width = 1,
                                          .height = 1,
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

const Renderer::EnvironmentGpuResource* Renderer::getOrCreateEnvironmentGpuResource(asset::AssetHandle handle)
{
    if (!handle.isValid()) {
        return nullptr;
    }

    const auto settings = readEnvironmentSettings(handle);
    if (!settings) {
        return nullptr;
    }

    const uint32_t environmentMipCount = fullMipLevelCount(DefaultEnvironmentCubeSize, DefaultEnvironmentCubeSize);
    const uint32_t prefilterMipCount = fullMipLevelCount(settings->prefilter_size, settings->prefilter_size);
    if (const auto it = m_environment_gpu_cache.find(handle); it != m_environment_gpu_cache.end()) {
        if (it->second.environment_size == DefaultEnvironmentCubeSize &&
            it->second.environment_mip_count == environmentMipCount &&
            it->second.irradiance_size == settings->irradiance_size &&
            it->second.prefilter_size == settings->prefilter_size &&
            it->second.prefilter_mip_count == prefilterMipCount) {
            return &it->second;
        }

        destroyEnvironmentGpuResource(it->second);
        m_environment_gpu_cache.erase(it);
    }

    const auto* texture = asset::AssetDatabase::get().get<interface::Texture>(handle);
    if (texture == nullptr || texture->getWidth() == 0 || texture->getHeight() == 0 || texture->getPixels().empty()) {
        return nullptr;
    }

    getOrCreateTextureGpuResource(handle, FallbackTexture::White);
    const auto sourceIt = m_texture_gpu_cache.find(handle);
    if (sourceIt == m_texture_gpu_cache.end() || !sourceIt->second.view || !sourceIt->second.sampler) {
        return nullptr;
    }
    const auto& sourceResource = sourceIt->second;

    EnvironmentGpuResource resource;
    resource.environment_size = DefaultEnvironmentCubeSize;
    resource.environment_mip_count = environmentMipCount;
    resource.irradiance_size = settings->irradiance_size;
    resource.prefilter_size = settings->prefilter_size;
    resource.prefilter_mip_count = prefilterMipCount;

    resource.environment.texture = createCubeTexture(*m_device,
                                                     resource.environment_size,
                                                     resource.environment_mip_count,
                                                     EnvironmentCubeFormat,
                                                     rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage |
                                                         rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst,
                                                     rhi::ResourceState::StorageReadWrite);
    resource.environment.view = createCubeTextureView(
        *m_device, resource.environment.texture, EnvironmentCubeFormat, resource.environment_mip_count);
    resource.environment.sampler = createCubeSampler(*m_device, resource.environment_mip_count);
    if (!resource.environment.texture || !resource.environment.view || !resource.environment.sampler) {
        destroyEnvironmentGpuResource(resource);
        return nullptr;
    }

    for (uint32_t face = 0; face < 6; ++face) {
        resource.environment_face_views[face] =
            createCubeFaceView(*m_device, resource.environment.texture, EnvironmentCubeFormat, 0, face);
        if (!resource.environment_face_views[face]) {
            destroyEnvironmentGpuResource(resource);
            return nullptr;
        }
    }

    resource.irradiance.texture =
        createCubeTexture(*m_device,
                          resource.irradiance_size,
                          1,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst,
                          rhi::ResourceState::StorageReadWrite);
    resource.prefilter.texture =
        createCubeTexture(*m_device,
                          resource.prefilter_size,
                          resource.prefilter_mip_count,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst,
                          rhi::ResourceState::StorageReadWrite);
    if (!resource.irradiance.texture || !resource.prefilter.texture) {
        destroyEnvironmentGpuResource(resource);
        return nullptr;
    }

    resource.irradiance.view =
        createCubeTextureView(*m_device, resource.irradiance.texture, FilteredEnvironmentCubeFormat, 1);
    resource.irradiance.sampler = createCubeSampler(*m_device, 1);
    resource.prefilter.view = createCubeTextureView(
        *m_device, resource.prefilter.texture, FilteredEnvironmentCubeFormat, resource.prefilter_mip_count);
    resource.prefilter.sampler = createCubeSampler(*m_device, resource.prefilter_mip_count);
    if (!resource.irradiance.view || !resource.irradiance.sampler || !resource.prefilter.view ||
        !resource.prefilter.sampler) {
        destroyEnvironmentGpuResource(resource);
        return nullptr;
    }

    for (uint32_t face = 0; face < 6; ++face) {
        resource.irradiance_face_views[face] =
            createCubeFaceView(*m_device, resource.irradiance.texture, FilteredEnvironmentCubeFormat, 0, face);
        if (!resource.irradiance_face_views[face]) {
            destroyEnvironmentGpuResource(resource);
            return nullptr;
        }
    }

    resource.prefilter_face_views.resize(static_cast<size_t>(resource.prefilter_mip_count) * 6);
    for (uint32_t mip = 0; mip < resource.prefilter_mip_count; ++mip) {
        for (uint32_t face = 0; face < 6; ++face) {
            const size_t index = static_cast<size_t>(mip) * 6 + face;
            resource.prefilter_face_views[index] =
                createCubeFaceView(*m_device, resource.prefilter.texture, FilteredEnvironmentCubeFormat, mip, face);
            if (!resource.prefilter_face_views[index]) {
                destroyEnvironmentGpuResource(resource);
                return nullptr;
            }
        }
    }

    const auto makeComputeBindGroupDesc =
        [&](rhi::TextureViewHandle sourceView, rhi::SamplerHandle sourceSampler, rhi::TextureViewHandle outputView) {
            return rhi::BindGroupDesc{
                .layout = m_environment_compute_bind_group_layout,
                .entries =
                    {
                        combinedTextureEntry(0, sourceView, sourceSampler),
                        rhi::BindGroupEntry{
                            .binding = 1,
                            .type = rhi::BindingType::StorageTexture,
                            .texture_view = outputView,
                        },
                    },
            };
        };

    const auto computeBindGroup = m_device->createBindGroup(
        makeComputeBindGroupDesc(sourceResource.view, sourceResource.sampler, resource.environment_face_views[0]));
    if (!computeBindGroup) {
        destroyEnvironmentGpuResource(resource);
        return nullptr;
    }

    const uint32_t environmentGroupCount =
        (resource.environment_size + EnvironmentComputeGroupSize - 1) / EnvironmentComputeGroupSize;
    m_compute_cmd->begin();

    const rhi::TextureTransition sourceRead{
        .texture = sourceResource.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    const rhi::TextureTransition environmentStorage{
        .texture = resource.environment.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&sourceRead, 1);
    m_compute_cmd->transition(&environmentStorage, 1);
    m_compute_cmd->setPipeline(m_environment_cubemap_pipeline);

    for (uint32_t face = 0; face < 6; ++face) {
        m_device->updateBindGroup(computeBindGroup,
                                  makeComputeBindGroupDesc(sourceResource.view,
                                                           sourceResource.sampler,
                                                           resource.environment_face_views[face]));

        const glm::vec4 constants{static_cast<float>(face), 0.0f, 0.0f, 0.0f};
        m_compute_cmd->setBindGroup(0, computeBindGroup);
        m_compute_cmd->pushConstants(rhi::shaderStageFlag(rhi::ShaderStage::Compute), 0, sizeof(constants), &constants);
        m_compute_cmd->dispatch(environmentGroupCount, environmentGroupCount);
    }

    const rhi::TextureTransition environmentCopyDst{
        .texture = resource.environment.texture,
        .state = rhi::ResourceState::CopyDst,
    };
    m_compute_cmd->transition(&environmentCopyDst, 1);
    m_compute_cmd->generateMipmaps(resource.environment.texture);
    const rhi::TextureTransition environmentRead{
        .texture = resource.environment.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&environmentRead, 1);

    const uint32_t irradianceGroupCount =
        (resource.irradiance_size + EnvironmentComputeGroupSize - 1) / EnvironmentComputeGroupSize;
    m_compute_cmd->setPipeline(m_environment_irradiance_pipeline);

    const rhi::TextureTransition irradianceStorage{
        .texture = resource.irradiance.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&irradianceStorage, 1);

    for (uint32_t face = 0; face < 6; ++face) {
        m_device->updateBindGroup(computeBindGroup,
                                  makeComputeBindGroupDesc(resource.environment.view,
                                                           resource.environment.sampler,
                                                           resource.irradiance_face_views[face]));

        const glm::vec4 constants{static_cast<float>(face), 0.0f, 0.0f, 0.0f};
        m_compute_cmd->setBindGroup(0, computeBindGroup);
        m_compute_cmd->pushConstants(rhi::shaderStageFlag(rhi::ShaderStage::Compute), 0, sizeof(constants), &constants);
        m_compute_cmd->dispatch(irradianceGroupCount, irradianceGroupCount);
    }

    const rhi::TextureTransition irradianceRead{
        .texture = resource.irradiance.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&irradianceRead, 1);

    m_compute_cmd->setPipeline(m_environment_prefilter_pipeline);
    const rhi::TextureTransition prefilterStorage{
        .texture = resource.prefilter.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&prefilterStorage, 1);

    const uint32_t maxPrefilterMip = lastMipLevel(resource.prefilter_mip_count);
    for (uint32_t mip = 0; mip < resource.prefilter_mip_count; ++mip) {
        const uint32_t mipSize = mipDimension(resource.prefilter_size, mip);
        const uint32_t groupCount = (mipSize + EnvironmentComputeGroupSize - 1) / EnvironmentComputeGroupSize;
        const float roughness =
            maxPrefilterMip > 0 ? std::min(static_cast<float>(mip) / static_cast<float>(maxPrefilterMip), 1.0f) : 0.0f;
        const uint32_t sampleCount = prefilterSampleCount(mip, maxPrefilterMip);

        for (uint32_t face = 0; face < 6; ++face) {
            const size_t index = static_cast<size_t>(mip) * 6 + face;
            m_device->updateBindGroup(computeBindGroup,
                                      makeComputeBindGroupDesc(resource.environment.view,
                                                               resource.environment.sampler,
                                                               resource.prefilter_face_views[index]));

            const glm::vec4 constants{static_cast<float>(face), roughness, static_cast<float>(sampleCount), 0.0f};
            m_compute_cmd->setBindGroup(0, computeBindGroup);
            m_compute_cmd->pushConstants(
                rhi::shaderStageFlag(rhi::ShaderStage::Compute), 0, sizeof(constants), &constants);
            m_compute_cmd->dispatch(groupCount, groupCount);
        }
    }

    const rhi::TextureTransition prefilterRead{
        .texture = resource.prefilter.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&prefilterRead, 1);
    m_compute_cmd->end();
    m_device->submit(m_compute_command_list);
    m_device->destroyBindGroup(computeBindGroup);

    return &m_environment_gpu_cache.emplace(handle, std::move(resource)).first->second;
}

void Renderer::createBlackEnvironmentGpuResource()
{
    constexpr float blackPixel[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const auto makeBlackHalfPixels = [](uint32_t size) {
        constexpr uint16_t halfFloatOne = 0x3c'00u;
        std::vector<uint16_t> pixels(static_cast<size_t>(size) * size * 4, 0u);
        for (size_t texel = 0; texel < static_cast<size_t>(size) * size; ++texel) {
            pixels[texel * 4 + 3] = halfFloatOne;
        }
        return pixels;
    };

    m_black_environment_gpu_resource.environment_size = 1;
    m_black_environment_gpu_resource.environment_mip_count = 1;
    m_black_environment_gpu_resource.environment.texture = createCubeTexture(
        *m_device, 1, 1, EnvironmentCubeFormat, rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst);
    m_black_environment_gpu_resource.environment.view = createCubeTextureView(
        *m_device, m_black_environment_gpu_resource.environment.texture, EnvironmentCubeFormat, 1);
    m_black_environment_gpu_resource.environment.sampler = createCubeSampler(*m_device, 1);
    if (m_black_environment_gpu_resource.environment.texture) {
        for (uint32_t face = 0; face < 6; ++face) {
            rhi_upload::uploadTextureData(*m_device,
                                          m_upload_command_list,
                                          m_black_environment_gpu_resource.environment.texture,
                                          rhi_upload::TextureUploadData{
                                              .data = blackPixel,
                                              .size = sizeof(blackPixel),
                                              .row_pitch = sizeof(blackPixel),
                                              .width = 1,
                                              .height = 1,
                                              .array_layer = face,
                                          });
        }
    }

    m_black_environment_gpu_resource.irradiance.texture =
        createCubeTexture(*m_device,
                          DefaultIrradianceCubeSize,
                          1,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst);
    m_black_environment_gpu_resource.irradiance.view = createCubeTextureView(
        *m_device, m_black_environment_gpu_resource.irradiance.texture, FilteredEnvironmentCubeFormat, 1);
    m_black_environment_gpu_resource.irradiance.sampler = createCubeSampler(*m_device, 1);
    if (m_black_environment_gpu_resource.irradiance.texture) {
        for (uint32_t face = 0; face < 6; ++face) {
            const uint32_t size = DefaultIrradianceCubeSize;
            const auto pixels = makeBlackHalfPixels(size);
            rhi_upload::uploadTextureData(*m_device,
                                          m_upload_command_list,
                                          m_black_environment_gpu_resource.irradiance.texture,
                                          rhi_upload::TextureUploadData{
                                              .data = pixels.data(),
                                              .size = pixels.size() * sizeof(uint16_t),
                                              .row_pitch = static_cast<size_t>(size) * sizeof(uint16_t) * 4,
                                              .width = size,
                                              .height = size,
                                              .array_layer = face,
                                          });
        }
    }

    const uint32_t prefilterMipLevels = fullMipLevelCount(DefaultPrefilterCubeSize, DefaultPrefilterCubeSize);
    m_black_environment_gpu_resource.prefilter.texture =
        createCubeTexture(*m_device,
                          DefaultPrefilterCubeSize,
                          prefilterMipLevels,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst);
    m_black_environment_gpu_resource.prefilter.view =
        createCubeTextureView(*m_device,
                              m_black_environment_gpu_resource.prefilter.texture,
                              FilteredEnvironmentCubeFormat,
                              prefilterMipLevels);
    m_black_environment_gpu_resource.prefilter.sampler = createCubeSampler(*m_device, prefilterMipLevels);
    for (uint32_t face = 0; face < 6; ++face) {
        m_black_environment_gpu_resource.environment_face_views[face] = createCubeFaceView(
            *m_device, m_black_environment_gpu_resource.environment.texture, EnvironmentCubeFormat, 0, face);
    }
    m_black_environment_gpu_resource.irradiance_size = DefaultIrradianceCubeSize;
    m_black_environment_gpu_resource.prefilter_size = DefaultPrefilterCubeSize;
    m_black_environment_gpu_resource.prefilter_mip_count = prefilterMipLevels;
    if (m_black_environment_gpu_resource.prefilter.texture) {
        for (uint32_t mip = 0; mip < prefilterMipLevels; ++mip) {
            const uint32_t mipSize = std::max(DefaultPrefilterCubeSize >> mip, 1u);
            const auto pixels = makeBlackHalfPixels(mipSize);
            for (uint32_t face = 0; face < 6; ++face) {
                rhi_upload::uploadTextureData(*m_device,
                                              m_upload_command_list,
                                              m_black_environment_gpu_resource.prefilter.texture,
                                              rhi_upload::TextureUploadData{
                                                  .data = pixels.data(),
                                                  .size = pixels.size() * sizeof(uint16_t),
                                                  .row_pitch = static_cast<size_t>(mipSize) * sizeof(uint16_t) * 4,
                                                  .width = mipSize,
                                                  .height = mipSize,
                                                  .mip_level = mip,
                                                  .array_layer = face,
                                              });
            }
        }
    }
}

void Renderer::createBrdfLutResource()
{
    m_brdf_lut_resource.texture = m_device->createTexture(rhi::TextureDesc{
        .width = BrdfLutSize,
        .height = BrdfLutSize,
        .dimension = rhi::TextureDimension::Texture2D,
        .format = rhi::TextureFormat::RG16F,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
        .mip_levels = 1,
        .array_layers = 1,
    });
    m_brdf_lut_resource.view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_brdf_lut_resource.texture,
        .view_dimension = rhi::TextureViewDimension::Texture2D,
        .format = rhi::TextureFormat::RG16F,
        .aspect = rhi::TextureAspect::Color,
        .mip_level_count = 1,
        .array_layer_count = 1,
    });
    m_brdf_lut_resource.sampler = m_device->createSampler(rhi::SamplerDesc{
        .min_filter = rhi::FilterMode::Linear,
        .mag_filter = rhi::FilterMode::Linear,
        .mip_filter = rhi::MipFilter::None,
        .address_u = rhi::AddressMode::ClampToEdge,
        .address_v = rhi::AddressMode::ClampToEdge,
        .address_w = rhi::AddressMode::ClampToEdge,
    });

    const auto computeShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = BrdfLutComputeShader,
    });
    const auto computeBindGroupLayout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::StorageTexture,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Compute),
                },
            },
    });
    const auto computePipelineLayout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {computeBindGroupLayout},
    });
    const auto computeBindGroup = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = computeBindGroupLayout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::StorageTexture,
                    .texture_view = m_brdf_lut_resource.view,
                },
            },
    });
    const auto computePipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = computePipelineLayout,
        .compute_shader = computeShader,
    });

    const auto destroyComputeResources = [&]() {
        if (computePipeline) {
            m_device->destroyPipeline(computePipeline);
        }
        if (computeBindGroup) {
            m_device->destroyBindGroup(computeBindGroup);
        }
        if (computePipelineLayout) {
            m_device->destroyPipelineLayout(computePipelineLayout);
        }
        if (computeBindGroupLayout) {
            m_device->destroyBindGroupLayout(computeBindGroupLayout);
        }
        if (computeShader) {
            m_device->destroyShader(computeShader);
        }
    };

    if (!m_brdf_lut_resource.texture || !m_brdf_lut_resource.view || !m_brdf_lut_resource.sampler || !computeShader ||
        !computeBindGroupLayout || !computePipelineLayout || !computeBindGroup || !computePipeline) {
        LUNA_CORE_ERROR("Failed to create BRDF LUT compute resources.");
        destroyComputeResources();
        destroyTextureGpuResource(m_brdf_lut_resource);
        return;
    }

    m_compute_cmd->begin();
    const rhi::TextureTransition brdfLutStorageTransition{
        .texture = m_brdf_lut_resource.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&brdfLutStorageTransition, 1);
    m_compute_cmd->setPipeline(computePipeline);
    m_compute_cmd->setBindGroup(0, computeBindGroup);
    m_compute_cmd->dispatch((BrdfLutSize + BrdfLutComputeGroupSize - 1) / BrdfLutComputeGroupSize,
                            (BrdfLutSize + BrdfLutComputeGroupSize - 1) / BrdfLutComputeGroupSize);
    const rhi::TextureTransition brdfLutShaderReadTransition{
        .texture = m_brdf_lut_resource.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&brdfLutShaderReadTransition, 1);
    m_compute_cmd->end();
    m_device->submit(m_compute_command_list);

    destroyComputeResources();
}

void Renderer::updateEnvironmentBindGroup(const EnvironmentGpuResource& resource)
{
    if (!m_environment_bind_group || !resource.environment.view || !resource.environment.sampler ||
        !resource.irradiance.view || !resource.irradiance.sampler || !resource.prefilter.view ||
        !resource.prefilter.sampler || !m_brdf_lut_resource.view || !m_brdf_lut_resource.sampler) {
        return;
    }

    m_device->updateBindGroup(
        m_environment_bind_group,
        rhi::BindGroupDesc{
            .layout = m_environment_bind_group_layout,
            .entries =
                {
                    combinedTextureEntry(0, resource.environment.view, resource.environment.sampler),
                    combinedTextureEntry(1, resource.irradiance.view, resource.irradiance.sampler),
                    combinedTextureEntry(2, resource.prefilter.view, resource.prefilter.sampler),
                    combinedTextureEntry(3, m_brdf_lut_resource.view, m_brdf_lut_resource.sampler),
                },
        });
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

void Renderer::destroyEnvironmentGpuResource(EnvironmentGpuResource& resource)
{
    for (const auto faceView : resource.environment_face_views) {
        if (faceView) {
            m_device->destroyTextureView(faceView);
        }
    }
    for (const auto faceView : resource.irradiance_face_views) {
        if (faceView) {
            m_device->destroyTextureView(faceView);
        }
    }
    for (const auto faceView : resource.prefilter_face_views) {
        if (faceView) {
            m_device->destroyTextureView(faceView);
        }
    }

    destroyTextureGpuResource(resource.environment);
    destroyTextureGpuResource(resource.irradiance);
    destroyTextureGpuResource(resource.prefilter);
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

    for (auto& [_, resource] : m_environment_gpu_cache) {
        destroyEnvironmentGpuResource(resource);
    }
    m_environment_gpu_cache.clear();

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
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.normal_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA16F,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.material_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.emission_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA16F,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.depth_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::Depth24Stencil8,
        .usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::DepthStencilWrite,
    });
    m_gbuffer.final_color_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
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
    LUNA_ASSERT(m_environment_bind_group, "Environment bind group is not initialized.");

    rhi::RenderPassBeginInfo pass;
    pass.color_attachments = {
        rhi::ColorAttachmentDesc{
            .view = m_gbuffer.albedo_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 0.0f},
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

    const rhi::TextureTransition barriers[] = {
        rhi::TextureTransition{
            .texture = m_gbuffer.albedo_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = m_gbuffer.normal_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = m_gbuffer.material_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = m_gbuffer.emission_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = m_gbuffer.depth_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
    };
    m_cmd->transition(barriers, 5);

    rhi::RenderPassBeginInfo lightingPass;
    lightingPass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = m_gbuffer.final_color_view,
        .load_op = rhi::LoadOp::Clear,
        .store_op = rhi::StoreOp::Store,
        .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
    });
    lightingPass.width = m_gbuffer.width;
    lightingPass.height = m_gbuffer.height;

    m_cmd->beginRenderPass(lightingPass);
    m_cmd->setPipeline(m_lighting_pipeline);
    m_cmd->setBindGroup(0, m_gbuffer.lighting_bind_group);
    m_cmd->setBindGroup(1, m_environment_bind_group);
    m_cmd->draw(3);
    m_cmd->endRenderPass();

    if (m_frameUniforms.environmentIntensity > 0.0f) {
        const rhi::TextureTransition depthAttachmentBarrier{
            .texture = m_gbuffer.depth_texture,
            .state = rhi::ResourceState::DepthStencilWrite,
        };
        m_cmd->transition(&depthAttachmentBarrier, 1);

        rhi::RenderPassBeginInfo skyboxPass;
        skyboxPass.color_attachments.push_back(rhi::ColorAttachmentDesc{
            .view = m_gbuffer.final_color_view,
            .load_op = rhi::LoadOp::Load,
            .store_op = rhi::StoreOp::Store,
        });
        skyboxPass.has_depth_stencil_attachment = true;
        skyboxPass.depth_stencil_attachment = rhi::DepthStencilAttachmentDesc{
            .view = m_gbuffer.depth_view,
            .depth_load_op = rhi::LoadOp::Load,
            .depth_store_op = rhi::StoreOp::DontCare,
        };
        skyboxPass.width = m_gbuffer.width;
        skyboxPass.height = m_gbuffer.height;

        m_cmd->beginRenderPass(skyboxPass);
        m_cmd->setPipeline(m_skybox_pipeline);
        m_cmd->setBindGroup(0, m_geometry_bind_group);
        m_cmd->setBindGroup(1, m_environment_bind_group);
        m_cmd->draw(3);
        m_cmd->endRenderPass();
    }

    const rhi::TextureTransition finalColorBarrier{
        .texture = m_gbuffer.final_color_texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_cmd->transition(&finalColorBarrier, 1);

    m_cmd->end();
    m_device->submit(m_command_list);
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

void Renderer::setLighting(const interface::RenderLighting& lighting)
{
    m_frameUniforms.directionalLightCount = lighting.directional_light_count > 0 ? 1u : 0u;
    const auto* environment = getOrCreateEnvironmentGpuResource(lighting.environment_map);
    const auto& activeEnvironment = environment != nullptr ? *environment : m_black_environment_gpu_resource;

    m_frameUniforms.environmentIntensity =
        environment != nullptr ? std::max(lighting.environment_intensity, 0.0f) : 0.0f;
    m_frameUniforms.maxPrefilterMip = static_cast<float>(lastMipLevel(activeEnvironment.prefilter_mip_count));
    updateEnvironmentBindGroup(activeEnvironment);
    m_cmd->setPipeline(m_geometry_pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);

    if (m_frameUniforms.directionalLightCount > 0) {
        m_frameUniforms.lightDir = lighting.directional_light.direction;
        m_frameUniforms.lightRadiance = lighting.directional_light.radiance;
    } else {
        m_frameUniforms.lightDir = {0.0f, -1.0f, 0.0f};
        m_frameUniforms.lightRadiance = {0.0f, 0.0f, 0.0f};
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
        if (modelMesh.submesh_start >= submeshes.size()) {
            continue;
        }

        const auto submeshStart = static_cast<size_t>(modelMesh.submesh_start);
        const auto availableSubmeshes = submeshes.size() - submeshStart;
        const auto requestedSubmeshes = modelMesh.submesh_count == std::numeric_limits<uint32_t>::max()
                                            ? availableSubmeshes
                                            : static_cast<size_t>(modelMesh.submesh_count);
        const auto submeshEnd = submeshStart + std::min(availableSubmeshes, requestedSubmeshes);
        const auto modelMeshTransform = transform * modelMesh.transform;

        for (size_t submeshIndex = submeshStart; submeshIndex < submeshEnd; ++submeshIndex) {
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
                drawSubMesh(*mesh, submeshIndex, submesh, *material, modelMeshTransform);
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
    m_objectUniforms.materialNormalScale = std::max(parameters.normal_scale, 0.0f);
    m_objectUniforms.materialOcclusionStrength = parameters.occlusion_strength;
    m_objectUniforms.materialHasNormalMap = parameters.normal_texture.isValid() ? 1u : 0u;
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

        gpu_mesh.vertex_buffer = m_device->createBuffer(rhi::BufferDesc{
            .size = gpu_mesh.vertex_buffer_capacity,
            .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
            .memory = rhi::MemoryUsage::CpuToGpu,
            .initial_state = rhi::ResourceState::VertexBuffer,
        });
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

        gpu_mesh.index_buffer = m_device->createBuffer(rhi::BufferDesc{
            .size = gpu_mesh.index_buffer_capacity,
            .usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst,
            .memory = rhi::MemoryUsage::CpuToGpu,
            .initial_state = rhi::ResourceState::IndexBuffer,
        });
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
