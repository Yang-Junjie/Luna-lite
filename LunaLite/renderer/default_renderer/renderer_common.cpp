#include "../../asset/asset_manager.h"
#include "renderer_common.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lunalite::renderer {
namespace {

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

} // namespace

const char BrdfLutComputeShader[] = R"GLSL(
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

size_t MaterialTextureKeyHash::operator()(const MaterialTextureKey& key) const noexcept
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

size_t growBufferCapacity(size_t current, size_t required)
{
    size_t capacity = current > 0 ? current : 256;
    while (capacity < required) {
        capacity *= 2;
    }

    return capacity;
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
                                     rhi::ResourceState initialState)
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

void destroyTextureGpuResource(rhi::Device& device, TextureGpuResource& resource)
{
    if (resource.sampler) {
        device.destroySampler(resource.sampler);
    }
    if (resource.view) {
        device.destroyTextureView(resource.view);
    }
    if (resource.texture) {
        device.destroyTexture(resource.texture);
    }
    resource = {};
}

void destroyEnvironmentGpuResource(rhi::Device& device, EnvironmentGpuResource& resource)
{
    for (const auto faceView : resource.environment_face_views) {
        if (faceView) {
            device.destroyTextureView(faceView);
        }
    }
    for (const auto faceView : resource.irradiance_face_views) {
        if (faceView) {
            device.destroyTextureView(faceView);
        }
    }
    for (const auto faceView : resource.prefilter_face_views) {
        if (faceView) {
            device.destroyTextureView(faceView);
        }
    }

    destroyTextureGpuResource(device, resource.environment);
    destroyTextureGpuResource(device, resource.irradiance);
    destroyTextureGpuResource(device, resource.prefilter);
    resource = {};
}

} // namespace lunalite::renderer
