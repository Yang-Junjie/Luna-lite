#pragma once
#include "../interface/frame_image.h"
#include "../interface/material.h"
#include "../interface/renderer.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <glm/glm.hpp>
#include <unordered_map>

namespace lunalite::renderer {
class Renderer : public interface::Renderer {
public:
    Renderer(rhi::Device& device, rhi::Swapchain& swapchain);
    ~Renderer() override;
    void beginFrame() override;
    void endFrame() override;
    void resize(uint32_t width, uint32_t height) override;
    void setViewProjection(const glm::mat4& view,
                           const glm::mat4& proj,
                           const glm::vec3& cameraPos,
                           float exposure) override;
    void setLighting(const interface::RenderLighting& lighting) override;
    void renderModel(const interface::Model& model, const glm::mat4& transform) override;
    void renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) override;
    const interface::FrameImage& getFrameImage() const override;

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
        float _pad3{0.0f};
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
        float _pad0{0.0f};
        float _pad1{0.0f};
    };

private:
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
        rhi::TextureHandle texture{};
        rhi::TextureViewHandle view{};
        rhi::SamplerHandle sampler{};
        rhi::TextureHandle irradiance_texture{};
        rhi::TextureViewHandle irradiance_view{};
        rhi::SamplerHandle irradiance_sampler{};
        rhi::TextureHandle prefilter_texture{};
        rhi::TextureViewHandle prefilter_view{};
        rhi::SamplerHandle prefilter_sampler{};
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

    void ensureGBuffer(uint32_t width, uint32_t height);
    void flushFrameUniforms();
    void drawSubMesh(const interface::Mesh& mesh,
                     size_t submesh_index,
                     const interface::SubMesh& submesh,
                     const interface::Material& material,
                     const glm::mat4& transform);
    MeshGpuData*
        getOrCreateSubMeshGpuData(const interface::Mesh& mesh, size_t submesh_index, const interface::SubMesh& submesh);
    uint64_t getSubMeshCacheKey(const interface::Mesh& mesh, size_t submesh_index) const;
    const TextureGpuResource& getOrCreateTextureGpuResource(asset::AssetHandle handle, FallbackTexture fallback);
    const TextureGpuResource& getOrCreateFallbackTexture(FallbackTexture fallback);
    const MaterialGpuResource& getOrCreateMaterialGpuResource(const interface::MaterialParameters& parameters);
    const EnvironmentGpuResource* getOrCreateEnvironmentGpuResource(asset::AssetHandle handle,
                                                                    const std::filesystem::path& environment_cube_path,
                                                                    const std::filesystem::path& irradiance_cube_path,
                                                                    const std::filesystem::path& prefilter_cube_path);
    void createBlackEnvironmentGpuResource();
    void createBrdfLutResource();
    void updateEnvironmentBindGroup(const EnvironmentGpuResource& resource);
    void destroyTextureGpuResource(TextureGpuResource& resource);
    void destroyEnvironmentGpuResource(EnvironmentGpuResource& resource);
    void destroyMaterialTextureResources();

    rhi::Device* m_device{nullptr};
    rhi::Swapchain* m_swapchain{nullptr};
    rhi::CommandListHandle m_command_list{};
    rhi::CommandList* m_cmd{nullptr};

    rhi::BindGroupLayoutHandle m_geometry_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_material_texture_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_lighting_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_environment_bind_group_layout{};

    rhi::PipelineLayoutHandle m_geometry_pipeline_layout{};
    rhi::PipelineLayoutHandle m_lighting_pipeline_layout{};
    rhi::PipelineLayoutHandle m_skybox_pipeline_layout{};
    rhi::PipelineHandle m_geometry_pipeline{};
    rhi::PipelineHandle m_lighting_pipeline{};
    rhi::PipelineHandle m_skybox_pipeline{};
    rhi::PipelineHandle m_line_pipeline{};

    rhi::BufferHandle m_frameUniformBuffer{};
    rhi::BufferHandle m_objectUniformBuffer{};
    rhi::BufferHandle m_line_vertex_buffer{};
    rhi::BindGroupHandle m_geometry_bind_group{};
    rhi::BindGroupHandle m_environment_bind_group{};
    rhi::SamplerHandle m_gbuffer_sampler{};
    TextureGpuResource m_white_fallback_texture{};
    TextureGpuResource m_flat_normal_fallback_texture{};
    TextureGpuResource m_brdf_lut_resource{};
    EnvironmentGpuResource m_black_environment_gpu_resource{};
    GBuffer m_gbuffer{};
    FrameUniforms m_frameUniforms;
    ObjectUniforms m_objectUniforms;
    interface::FrameImage m_frame_image;
    bool m_frame_uniforms_dirty{true};
    std::unordered_map<uint64_t, MeshGpuData> m_mesh_gpu_cache;
    std::unordered_map<asset::AssetHandle, TextureGpuResource> m_texture_gpu_cache;
    std::unordered_map<asset::AssetHandle, EnvironmentGpuResource> m_environment_gpu_cache;
    std::unordered_map<MaterialTextureKey, MaterialGpuResource, MaterialTextureKeyHash> m_material_gpu_cache;
};
} // namespace lunalite::renderer
