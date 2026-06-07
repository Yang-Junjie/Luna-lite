#pragma once
#include "../../asset/asset.h"
#include "../interface/texture.h"
#include "TinyRHI/interface/device.h"

#include <cstddef>
#include <cstdint>

#include <array>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

namespace lunalite::renderer {

constexpr uint32_t MaxShadowCascades = 4;

struct LineVertex {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
};

struct alignas(16) FrameUniforms {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 cameraPos{0.0f};
    float _pad0{0.0f};
    glm::vec3 lightDir{0.0f, -1.0f, 0.0f};
    float _pad1{0.0f};
    glm::vec3 lightRadiance{0.0f};
    float _pad2{0.0f};
    uint32_t directionalLightCount{0};
    float environmentIntensity{1.0f};
    float maxPrefilterMip{0.0f};
    float _pad4{0.0f};
    glm::mat4 inverseViewProjection{1.0f};
    float exposure{1.0f};
    float _pad5{0.0f};
    float _pad6{0.0f};
    float _pad7{0.0f};
};

struct alignas(16) ObjectUniforms {
    glm::mat4 model{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec4 materialAlbedo{0.8f, 0.65f, 0.5f, 1.0f};
    glm::vec3 materialEmission{0.0f};
    float materialEmissionStrength{0.0f};
    float materialMetallic{0.0f};
    float materialRoughness{0.5f};
    uint32_t materialShadingModel{0};
    float materialNormalScale{1.0f};
    float materialOcclusionStrength{1.0f};
    uint32_t materialHasNormalMap{0};
    float _pad1{0.0f};
};

struct alignas(16) ShadowFrameUniforms {
    glm::mat4 lightViewProjection{1.0f};
};

struct alignas(16) ShadowObjectUniforms {
    glm::mat4 model{1.0f};
};

struct alignas(16) ShadowLightingUniforms {
    std::array<glm::mat4, MaxShadowCascades> lightViewProjections{
        glm::mat4{1.0f},
        glm::mat4{1.0f},
        glm::mat4{1.0f},
        glm::mat4{1.0f},
    };
    glm::vec4 cascadeSplits{0.0f};
    glm::vec4 texelSizeBiasNormalBias{0.0f};
    glm::vec4 cascadeBlendPadding{0.0f};
    glm::uvec4 enabledPcfRadiusCascadeCountPadding{0u};
};

struct ShadowCascade {
    glm::mat4 light_view_projection{1.0f};
    float split_depth{0.0f};
    std::vector<uint32_t> caster_mesh_indices;
};

struct ShadowCascadeData {
    std::array<ShadowCascade, MaxShadowCascades> cascades{};
    uint32_t count{0};
};

struct MeshGpuData {
    rhi::BufferHandle vertex_buffer{};
    rhi::BufferHandle index_buffer{};

    bool vertex_buffer_dynamic{false};
    bool index_buffer_dynamic{false};
    size_t vertex_buffer_capacity{0};
    size_t index_buffer_capacity{0};
    uint64_t uploaded_vertex_version{0};
    uint64_t uploaded_index_version{0};

    uint32_t vertex_count{0};
    uint32_t index_count{0};
};

struct GBuffer {
    uint32_t width{0};
    uint32_t height{0};

    rhi::TextureHandle albedo_texture{};
    rhi::TextureHandle normal_texture{};
    rhi::TextureHandle material_texture{};
    rhi::TextureHandle emission_texture{};
    rhi::TextureHandle depth_texture{};
    rhi::TextureHandle final_color_texture{};

    rhi::TextureViewHandle albedo_view{};
    rhi::TextureViewHandle normal_view{};
    rhi::TextureViewHandle material_view{};
    rhi::TextureViewHandle emission_view{};
    rhi::TextureViewHandle depth_view{};
    rhi::TextureViewHandle final_color_view{};

    rhi::BindGroupHandle lighting_bind_group{};
};

enum class FallbackTexture {
    White,
    FlatNormal
};

struct TextureGpuResource {
    rhi::TextureHandle texture{};
    rhi::TextureViewHandle view{};
    rhi::SamplerHandle sampler{};
};

struct EnvironmentGpuResource {
    TextureGpuResource environment{};
    TextureGpuResource irradiance{};
    TextureGpuResource prefilter{};
    uint32_t environment_size{0};
    uint32_t environment_mip_count{0};
    uint32_t irradiance_size{0};
    uint32_t prefilter_size{0};
    uint32_t prefilter_mip_count{0};
    std::array<rhi::TextureViewHandle, 6> environment_face_views{};
    std::array<rhi::TextureViewHandle, 6> irradiance_face_views{};
    std::vector<rhi::TextureViewHandle> prefilter_face_views;
};

struct MaterialTextureKey {
    asset::AssetHandle albedo{};
    asset::AssetHandle normal{};
    asset::AssetHandle metallic_roughness{};
    asset::AssetHandle occlusion{};
    asset::AssetHandle emission{};

    bool operator==(const MaterialTextureKey& other) const
    {
        return albedo == other.albedo && normal == other.normal && metallic_roughness == other.metallic_roughness &&
               occlusion == other.occlusion && emission == other.emission;
    }
};

struct MaterialTextureKeyHash {
    size_t operator()(const MaterialTextureKey& key) const noexcept;
};

struct MaterialGpuResource {
    rhi::BindGroupHandle bind_group{};
};

struct EnvironmentSettings {
    uint32_t irradiance_size{32};
    uint32_t prefilter_size{128};
};

constexpr uint32_t DefaultEnvironmentCubeSize = 512;
constexpr uint32_t DefaultIrradianceCubeSize = 32;
constexpr uint32_t DefaultPrefilterCubeSize = 128;
constexpr uint32_t EnvironmentComputeGroupSize = 8;
constexpr uint32_t BrdfLutSize = 512;
constexpr uint32_t BrdfLutComputeGroupSize = 8;
constexpr rhi::TextureFormat EnvironmentCubeFormat = rhi::TextureFormat::RGBA32F;
constexpr rhi::TextureFormat FilteredEnvironmentCubeFormat = rhi::TextureFormat::RGBA16F;

extern const char BrdfLutComputeShader[];

size_t growBufferCapacity(size_t current, size_t required);
std::string loadDefaultRendererShader(const char* file_name);
uint32_t fullMipLevelCount(uint32_t width, uint32_t height);
rhi::TextureFormat toRHITextureFormat(interface::TextureFormat format,
                                      const interface::TextureImportSettings& settings);
size_t textureBytesPerPixel(interface::TextureFormat format);
rhi::SamplerDesc toRHISamplerDesc(const interface::TextureSamplerDesc& sampler);
rhi::SamplerDesc fallbackSamplerDesc();
rhi::BindGroupEntry combinedTextureEntry(uint32_t binding, rhi::TextureViewHandle view, rhi::SamplerHandle sampler);
std::optional<EnvironmentSettings> readEnvironmentSettings(asset::AssetHandle handle);
rhi::TextureHandle createCubeTexture(rhi::Device& device,
                                     uint32_t size,
                                     uint32_t mipCount,
                                     rhi::TextureFormat format,
                                     rhi::TextureUsage usage,
                                     rhi::ResourceState initialState = rhi::ResourceState::CopyDst);
rhi::TextureViewHandle createCubeTextureView(rhi::Device& device,
                                             rhi::TextureHandle texture,
                                             rhi::TextureFormat format,
                                             uint32_t mipCount);
rhi::TextureViewHandle createCubeFaceView(
    rhi::Device& device, rhi::TextureHandle texture, rhi::TextureFormat format, uint32_t mipLevel, uint32_t face);
rhi::SamplerHandle createCubeSampler(rhi::Device& device, uint32_t mipCount);
uint32_t mipDimension(uint32_t base, uint32_t mip);
uint32_t lastMipLevel(uint32_t mipLevels);
uint32_t prefilterSampleCount(uint32_t mip, uint32_t maxMip);
void destroyTextureGpuResource(rhi::Device& device, TextureGpuResource& resource);
void destroyEnvironmentGpuResource(rhi::Device& device, EnvironmentGpuResource& resource);

} // namespace lunalite::renderer
