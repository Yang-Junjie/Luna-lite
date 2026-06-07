#pragma once
#include "../../interface/frame_render_data.h"
#include "../../interface/mesh.h"
#include "../renderer_common.h"
#include "../shadow_map_resource.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/device.h"

#include <cstddef>
#include <cstdint>

#include <array>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace lunalite::renderer {

class ShadowPass {
public:
    ShadowPass(rhi::Device& device,
               rhi::CommandList& commands,
               rhi::PipelineHandle shadow_pipeline,
               rhi::BindGroupLayoutHandle shadow_bind_group_layout);
    ~ShadowPass();

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;

    std::array<uint32_t, MaxShadowCascades> execute(const ShadowMapResource& shadow_map,
                                                    const ShadowCascadeData& cascade_data,
                                                    const std::vector<interface::MeshDrawCommand>& mesh_commands);

private:
    uint32_t renderMesh(const interface::MeshDrawCommand& mesh_command);
    bool drawSubMesh(const interface::Mesh& mesh,
                     size_t submesh_index,
                     const interface::SubMesh& submesh,
                     const glm::mat4& transform);
    MeshGpuData*
        getOrCreateSubMeshGpuData(const interface::Mesh& mesh, size_t submesh_index, const interface::SubMesh& submesh);
    uint64_t getSubMeshCacheKey(const interface::Mesh& mesh, size_t submesh_index) const;

    rhi::Device* m_device{nullptr};
    rhi::CommandList* m_cmd{nullptr};
    rhi::PipelineHandle m_shadow_pipeline{};
    rhi::BindGroupLayoutHandle m_shadow_bind_group_layout{};
    rhi::BufferHandle m_shadow_frame_uniform_buffer{};
    rhi::BufferHandle m_shadow_object_uniform_buffer{};
    rhi::BindGroupHandle m_shadow_bind_group{};
    ShadowFrameUniforms m_frame_uniforms;
    ShadowObjectUniforms m_object_uniforms;
    std::unordered_map<uint64_t, MeshGpuData> m_mesh_gpu_cache;
};

} // namespace lunalite::renderer
