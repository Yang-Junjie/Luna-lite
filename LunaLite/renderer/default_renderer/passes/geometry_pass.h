#pragma once
#include "../../interface/frame_render_data.h"
#include "../../interface/material.h"
#include "../../interface/mesh.h"
#include "../material_gpu_cache.h"
#include "../renderer_common.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/device.h"

#include <cstddef>
#include <cstdint>

#include <unordered_map>

namespace lunalite::renderer {

class GeometryPass {
public:
    GeometryPass(rhi::Device& device,
                 rhi::CommandList& commands,
                 rhi::PipelineHandle geometry_pipeline,
                 rhi::BindGroupHandle geometry_bind_group,
                 rhi::BufferHandle frame_uniform_buffer,
                 rhi::BufferHandle object_uniform_buffer,
                 MaterialGpuCache& material_cache,
                 FrameUniforms& frame_uniforms,
                 bool& frame_uniforms_dirty);
    ~GeometryPass();

    GeometryPass(const GeometryPass&) = delete;
    GeometryPass& operator=(const GeometryPass&) = delete;

    void begin(const GBuffer& gbuffer);
    void end();
    uint32_t renderMesh(const interface::MeshDrawCommand& mesh_command);

private:
    bool drawSubMesh(const interface::Mesh& mesh,
                     size_t submesh_index,
                     const interface::SubMesh& submesh,
                     const interface::Material& material,
                     const glm::mat4& transform);
    void flushFrameUniforms();
    MeshGpuData*
        getOrCreateSubMeshGpuData(const interface::Mesh& mesh, size_t submesh_index, const interface::SubMesh& submesh);
    uint64_t getSubMeshCacheKey(const interface::Mesh& mesh, size_t submesh_index) const;

    rhi::Device* m_device{nullptr};
    rhi::CommandList* m_cmd{nullptr};
    rhi::PipelineHandle m_geometry_pipeline{};
    rhi::BindGroupHandle m_geometry_bind_group{};
    rhi::BufferHandle m_frame_uniform_buffer{};
    rhi::BufferHandle m_object_uniform_buffer{};
    MaterialGpuCache* m_material_cache{nullptr};
    FrameUniforms* m_frame_uniforms{nullptr};
    bool* m_frame_uniforms_dirty{nullptr};
    ObjectUniforms m_object_uniforms;
    std::unordered_map<uint64_t, MeshGpuData> m_mesh_gpu_cache;
};

} // namespace lunalite::renderer
