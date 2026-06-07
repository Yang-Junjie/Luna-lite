#include "../../../asset/asset_database.h"
#include "../../../core/log.h"
#include "shadow_pass.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace lunalite::renderer {

ShadowPass::ShadowPass(rhi::Device& device,
                       rhi::CommandList& commands,
                       rhi::PipelineHandle shadow_pipeline,
                       rhi::BindGroupLayoutHandle shadow_bind_group_layout)
    : m_device(&device),
      m_cmd(&commands),
      m_shadow_pipeline(shadow_pipeline),
      m_shadow_bind_group_layout(shadow_bind_group_layout)
{
    m_shadow_frame_uniform_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(ShadowFrameUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::UniformRead,
    });
    m_shadow_object_uniform_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(ShadowObjectUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::UniformRead,
    });
    m_shadow_bind_group = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_shadow_bind_group_layout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_shadow_frame_uniform_buffer,
                            .offset = 0,
                            .size = sizeof(ShadowFrameUniforms),
                        },
                },
                rhi::BindGroupEntry{
                    .binding = 1,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_shadow_object_uniform_buffer,
                            .offset = 0,
                            .size = sizeof(ShadowObjectUniforms),
                        },
                },
            },
    });

    LUNA_ASSERT(m_shadow_frame_uniform_buffer, "Failed to create shadow frame uniform buffer.");
    LUNA_ASSERT(m_shadow_object_uniform_buffer, "Failed to create shadow object uniform buffer.");
    LUNA_ASSERT(m_shadow_bind_group, "Failed to create shadow bind group.");
}

ShadowPass::~ShadowPass()
{
    for (auto& [_, gpu_mesh] : m_mesh_gpu_cache) {
        if (gpu_mesh.vertex_buffer) {
            m_device->destroyBuffer(gpu_mesh.vertex_buffer);
        }
        if (gpu_mesh.index_buffer) {
            m_device->destroyBuffer(gpu_mesh.index_buffer);
        }
    }
    m_mesh_gpu_cache.clear();

    if (m_shadow_bind_group) {
        m_device->destroyBindGroup(m_shadow_bind_group);
        m_shadow_bind_group = {};
    }
    if (m_shadow_object_uniform_buffer) {
        m_device->destroyBuffer(m_shadow_object_uniform_buffer);
        m_shadow_object_uniform_buffer = {};
    }
    if (m_shadow_frame_uniform_buffer) {
        m_device->destroyBuffer(m_shadow_frame_uniform_buffer);
        m_shadow_frame_uniform_buffer = {};
    }
}

std::array<uint32_t, MaxShadowCascades>
    ShadowPass::execute(const ShadowMapResource& shadow_map,
                        const ShadowCascadeData& cascade_data,
                        const std::vector<interface::MeshDrawCommand>& mesh_commands)
{
    std::array<uint32_t, MaxShadowCascades> drawCalls{};
    const auto cascadeCount = std::min(cascade_data.count, shadow_map.cascadeCount());
    if (!shadow_map.view() || shadow_map.size() == 0 || cascadeCount == 0 || mesh_commands.empty()) {
        return drawCalls;
    }

    for (uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex) {
        const auto layerView = shadow_map.layerView(cascadeIndex);
        if (!layerView) {
            continue;
        }

        m_frame_uniforms.lightViewProjection = cascade_data.cascades[cascadeIndex].light_view_projection;
        m_device->updateBuffer(m_shadow_frame_uniform_buffer, 0, &m_frame_uniforms, sizeof(ShadowFrameUniforms));

        rhi::RenderPassBeginInfo pass;
        pass.has_depth_stencil_attachment = true;
        pass.depth_stencil_attachment = rhi::DepthStencilAttachmentDesc{
            .view = layerView,
            .depth_load_op = rhi::LoadOp::Clear,
            .depth_store_op = rhi::StoreOp::Store,
            .clear_depth = 1.0f,
        };
        pass.width = shadow_map.size();
        pass.height = shadow_map.size();

        m_cmd->beginRenderPass(pass);
        m_cmd->setPipeline(m_shadow_pipeline);
        m_cmd->setBindGroup(0, m_shadow_bind_group);

        for (const auto meshIndex : cascade_data.cascades[cascadeIndex].caster_mesh_indices) {
            if (meshIndex >= mesh_commands.size()) {
                continue;
            }

            drawCalls[cascadeIndex] += renderMesh(mesh_commands[meshIndex]);
        }

        m_cmd->endRenderPass();
    }

    return drawCalls;
}

uint32_t ShadowPass::renderMesh(const interface::MeshDrawCommand& mesh_command)
{
    if (!mesh_command.mesh.isValid()) {
        return 0;
    }

    const auto* mesh = asset::AssetDatabase::get().get<interface::Mesh>(mesh_command.mesh);
    if (mesh == nullptr) {
        LUNA_CORE_ERROR("Mesh instance {} has a null mesh asset", static_cast<uint64_t>(mesh_command.mesh));
        return 0;
    }

    const auto& submeshes = mesh->getSubMeshes();
    if (mesh_command.submesh_start >= submeshes.size()) {
        return 0;
    }

    const auto submeshStart = static_cast<size_t>(mesh_command.submesh_start);
    const auto availableSubmeshes = submeshes.size() - submeshStart;
    const auto requestedSubmeshes = mesh_command.submesh_count == std::numeric_limits<uint32_t>::max()
                                        ? availableSubmeshes
                                        : static_cast<size_t>(mesh_command.submesh_count);
    const auto submeshEnd = submeshStart + std::min(availableSubmeshes, requestedSubmeshes);
    uint32_t drawCalls = 0;

    for (size_t submeshIndex = submeshStart; submeshIndex < submeshEnd; ++submeshIndex) {
        if (drawSubMesh(*mesh, submeshIndex, submeshes[submeshIndex], mesh_command.transform)) {
            drawCalls += 1;
        }
    }

    return drawCalls;
}

bool ShadowPass::drawSubMesh(const interface::Mesh& mesh,
                             size_t submesh_index,
                             const interface::SubMesh& submesh,
                             const glm::mat4& transform)
{
    auto* gpu_mesh = getOrCreateSubMeshGpuData(mesh, submesh_index, submesh);
    if (gpu_mesh == nullptr || !gpu_mesh->vertex_buffer || gpu_mesh->vertex_count == 0) {
        return false;
    }

    m_object_uniforms.model = transform;
    m_device->updateBuffer(m_shadow_object_uniform_buffer, 0, &m_object_uniforms, sizeof(ShadowObjectUniforms));

    m_cmd->setVertexBuffer(0, gpu_mesh->vertex_buffer);
    if (gpu_mesh->index_buffer && gpu_mesh->index_count > 0) {
        m_cmd->setIndexBuffer(gpu_mesh->index_buffer, rhi::IndexFormat::UInt32);
        m_cmd->drawIndexed(gpu_mesh->index_count);
    } else {
        m_cmd->draw(gpu_mesh->vertex_count);
    }
    return true;
}

MeshGpuData* ShadowPass::getOrCreateSubMeshGpuData(const interface::Mesh& mesh,
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
        const bool creatingVertexBuffer = !gpu_mesh.vertex_buffer;
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
        LUNA_ASSERT(gpu_mesh.vertex_buffer, "Failed to create shadow mesh vertex buffer.");
        gpu_mesh.uploaded_vertex_version = 0;
        LUNA_CORE_DEBUG("{} shadow mesh vertex buffer for asset {} submesh {} (vertices: {}, capacity: {} bytes)",
                        creatingVertexBuffer ? "Created" : "Recreated",
                        mesh.handle.toString(),
                        submesh_index,
                        vertices.size(),
                        gpu_mesh.vertex_buffer_capacity);
    }

    if (gpu_mesh.uploaded_vertex_version != vertex_version) {
        m_device->updateBuffer(gpu_mesh.vertex_buffer, 0, vertices.data(), vertex_buffer_size);
        gpu_mesh.uploaded_vertex_version = vertex_version;
    }

    if (!indices.empty() && (!gpu_mesh.index_buffer || gpu_mesh.index_buffer_capacity < index_buffer_size ||
                             (index_changed && !gpu_mesh.index_buffer_dynamic))) {
        const bool creatingIndexBuffer = !gpu_mesh.index_buffer;
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
        LUNA_ASSERT(gpu_mesh.index_buffer, "Failed to create shadow mesh index buffer.");
        gpu_mesh.uploaded_index_version = 0;
        LUNA_CORE_DEBUG("{} shadow mesh index buffer for asset {} submesh {} (indices: {}, capacity: {} bytes)",
                        creatingIndexBuffer ? "Created" : "Recreated",
                        mesh.handle.toString(),
                        submesh_index,
                        indices.size(),
                        gpu_mesh.index_buffer_capacity);
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

uint64_t ShadowPass::getSubMeshCacheKey(const interface::Mesh& mesh, size_t submesh_index) const
{
    const auto handle = static_cast<uint64_t>(mesh.handle);
    LUNA_ASSERT(handle != 0);

    return handle ^ ((static_cast<uint64_t>(submesh_index) + 1u) * 0x9E'37'79'B9'7F'4A'7C'15ull);
}

} // namespace lunalite::renderer
