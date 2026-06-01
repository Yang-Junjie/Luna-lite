#pragma once
#include "../interface/frame_image.h"
#include "../interface/material.h"
#include "../interface/renderer.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <unordered_map>

namespace lunalite::renderer {
class Renderer : public interface::Renderer {
public:
    Renderer(rhi::Device& device, rhi::Swapchain& swapchain);
    ~Renderer() override = default;
    void beginFrame() override;
    void endFrame() override;
    void resize(uint32_t width, uint32_t height) override;
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) override;
    void setSceneLighting(const interface::SceneLighting& lighting) override;
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
        glm::vec3 lightAmbient{0.0f};
        float _pad2{0.0f};
        glm::vec3 lightDiffuse{0.0f};
        float _pad3{0.0f};
        glm::vec3 lightSpecular{0.0f};
        float _pad4{0.0f};
        uint32_t directionalLightCount{0};
        float _pad5{0.0f};
        float _pad6{0.0f};
        float _pad7{0.0f};
        glm::vec3 environmentAmbient{0.0f};
        float _pad8{0.0f};
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
        float _pad0{0.0f};
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
        rhi::TextureHandle depth_texture{};
        rhi::TextureHandle final_color_texture{};

        rhi::TextureViewHandle albedo_view{};
        rhi::TextureViewHandle normal_view{};
        rhi::TextureViewHandle material_view{};
        rhi::TextureViewHandle depth_view{};
        rhi::TextureViewHandle final_color_view{};

        rhi::BindGroupHandle lighting_bind_group{};
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

    rhi::Device* m_device{nullptr};
    rhi::Swapchain* m_swapchain{nullptr};
    rhi::CommandList* m_cmd{nullptr};

    rhi::BindGroupLayoutHandle m_geometry_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_lighting_bind_group_layout{};

    rhi::PipelineLayoutHandle m_geometry_pipeline_layout{};
    rhi::PipelineLayoutHandle m_lighting_pipeline_layout{};
    rhi::PipelineHandle m_geometry_pipeline{};
    rhi::PipelineHandle m_lighting_pipeline{};
    rhi::PipelineHandle m_line_pipeline{};

    rhi::BufferHandle m_frameUniformBuffer{};
    rhi::BufferHandle m_objectUniformBuffer{};
    rhi::BufferHandle m_line_vertex_buffer{};
    rhi::BindGroupHandle m_geometry_bind_group{};
    rhi::SamplerHandle m_gbuffer_sampler{};
    GBuffer m_gbuffer{};
    FrameUniforms m_frameUniforms;
    ObjectUniforms m_objectUniforms;
    interface::Material m_default_material;
    interface::FrameImage m_frame_image;
    bool m_frame_uniforms_dirty{true};
    std::unordered_map<uint64_t, MeshGpuData> m_mesh_gpu_cache;
};
} // namespace lunalite::renderer
