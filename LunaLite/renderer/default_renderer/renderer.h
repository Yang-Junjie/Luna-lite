#pragma once
#include "TinyRHI/interface/instance.h"
#include "../interface/frame_image.h"
#include "../interface/renderer.h"

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <unordered_map>

namespace lunalite::renderer {
class Renderer : public interface::Renderer {
public:
    explicit Renderer(rhi::Instance& instance);
    ~Renderer() override = default;
    void beginFrame() override;
    void endFrame() override;
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) override;
    void setDirectionalLight(const glm::vec3& direction,
                             const glm::vec3& ambient,
                             const glm::vec3& diffuse,
                             const glm::vec3& specular) override;
    void renderMesh(const interface::Mesh& mesh, const glm::mat4& transform) override;
    const interface::FrameImage& getFrameImage() const override;

    struct alignas(16) FrameUniforms {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        glm::vec3 cameraPos{0.0f};
        float _pad0{0.0f};
        glm::vec3 lightDir{0.0f, -1.0f, 0.0f};
        float _pad1{0.0f};
        glm::vec3 lightAmbient{0.05f, 0.05f, 0.05f};
        float _pad2{0.0f};
        glm::vec3 lightDiffuse{0.8f, 0.8f, 0.8f};
        float _pad3{0.0f};
        glm::vec3 lightSpecular{1.0f, 1.0f, 1.0f};
        float _pad4{0.0f};
    };

    struct alignas(16) ObjectUniforms {
        glm::mat4 model{1.0f};
        glm::mat4 normalMatrix{1.0f};
    };

private:
    struct MeshGpuData {
        rhi::BufferHandle vertex_buffer{0};
        rhi::BufferHandle index_buffer{0};

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

        rhi::TextureHandle albedo_texture{0};
        rhi::TextureHandle normal_texture{0};
        rhi::TextureHandle material_texture{0};
        rhi::TextureHandle depth_texture{0};
        rhi::TextureHandle final_color_texture{0};

        rhi::TextureViewHandle albedo_view{0};
        rhi::TextureViewHandle normal_view{0};
        rhi::TextureViewHandle material_view{0};
        rhi::TextureViewHandle depth_view{0};
        rhi::TextureViewHandle final_color_view{0};

        rhi::BindGroupHandle lighting_bind_group{0};
    };

    void ensureGBuffer(uint32_t width, uint32_t height);
    void flushFrameUniforms();
    MeshGpuData* getOrCreateMeshGpuData(const interface::Mesh& mesh);
    uint64_t getMeshCacheKey(const interface::Mesh& mesh) const;

    rhi::Device* m_device{nullptr};
    rhi::Swapchain* m_swapchain{nullptr};
    rhi::CommandList* m_cmd{nullptr};

    rhi::BindGroupLayoutHandle m_geometry_bind_group_layout{0};
    rhi::BindGroupLayoutHandle m_lighting_bind_group_layout{0};

    rhi::PipelineLayoutHandle m_geometry_pipeline_layout{0};
    rhi::PipelineLayoutHandle m_lighting_pipeline_layout{0};
    rhi::PipelineHandle m_geometry_pipeline{0};
    rhi::PipelineHandle m_lighting_pipeline{0};

    rhi::BufferHandle m_frameUniformBuffer{0};
    rhi::BufferHandle m_objectUniformBuffer{0};
    rhi::BindGroupHandle m_geometry_bind_group{0};
    rhi::SamplerHandle m_gbuffer_sampler{0};
    GBuffer m_gbuffer{};
    FrameUniforms m_frameUniforms;
    ObjectUniforms m_objectUniforms;
    interface::FrameImage m_frame_image;
    bool m_frame_uniforms_dirty{true};
    std::unordered_map<uint64_t, MeshGpuData> m_mesh_gpu_cache;
};
} // namespace lunalite::renderer
