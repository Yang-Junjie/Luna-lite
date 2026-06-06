#include "../../../asset/asset_database.h"
#include "../../../asset/builtin/builtin_assets.h"
#include "../../../core/log.h"
#include "geometry_pass.h"

#include <algorithm>
#include <limits>

namespace lunalite::renderer {

GeometryPass::GeometryPass(rhi::Device& device,
                           rhi::CommandList& commands,
                           rhi::PipelineHandle geometry_pipeline,
                           rhi::BindGroupHandle geometry_bind_group,
                           rhi::BufferHandle frame_uniform_buffer,
                           rhi::BufferHandle object_uniform_buffer,
                           MaterialGpuCache& material_cache,
                           FrameUniforms& frame_uniforms,
                           bool& frame_uniforms_dirty)
    : m_device(&device),
      m_cmd(&commands),
      m_geometry_pipeline(geometry_pipeline),
      m_geometry_bind_group(geometry_bind_group),
      m_frame_uniform_buffer(frame_uniform_buffer),
      m_object_uniform_buffer(object_uniform_buffer),
      m_material_cache(&material_cache),
      m_frame_uniforms(&frame_uniforms),
      m_frame_uniforms_dirty(&frame_uniforms_dirty)
{}

GeometryPass::~GeometryPass()
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
}

void GeometryPass::begin(const GBuffer& gbuffer)
{
    rhi::RenderPassBeginInfo pass;
    pass.color_attachments = {
        rhi::ColorAttachmentDesc{
            .view = gbuffer.albedo_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 0.0f},
        },
        rhi::ColorAttachmentDesc{
            .view = gbuffer.normal_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.5f, 0.5f, 1.0f, 1.0f},
        },
        rhi::ColorAttachmentDesc{
            .view = gbuffer.material_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
        },
        rhi::ColorAttachmentDesc{
            .view = gbuffer.emission_view,
            .load_op = rhi::LoadOp::Clear,
            .store_op = rhi::StoreOp::Store,
            .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
        },
    };
    pass.has_depth_stencil_attachment = true;
    pass.depth_stencil_attachment = rhi::DepthStencilAttachmentDesc{
        .view = gbuffer.depth_view,
        .depth_load_op = rhi::LoadOp::Clear,
        .depth_store_op = rhi::StoreOp::Store,
        .clear_depth = 1.0f,
    };
    pass.width = gbuffer.width;
    pass.height = gbuffer.height;

    m_cmd->beginRenderPass(pass);
    m_cmd->setPipeline(m_geometry_pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);
}

void GeometryPass::end()
{
    flushFrameUniforms();
    m_cmd->endRenderPass();
}

void GeometryPass::renderMesh(const interface::MeshDrawCommand& mesh_instance)
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

    if (!mesh_instance.mesh.isValid()) {
        return;
    }

    const auto* mesh = asset::AssetDatabase::get().get<interface::Mesh>(mesh_instance.mesh);
    if (mesh == nullptr) {
        LUNA_CORE_ERROR("Mesh instance {} has a null mesh asset", static_cast<uint64_t>(mesh_instance.mesh));
        return;
    }

    const auto& submeshes = mesh->getSubMeshes();
    if (mesh_instance.submesh_start >= submeshes.size()) {
        return;
    }

    const auto submeshStart = static_cast<size_t>(mesh_instance.submesh_start);
    const auto availableSubmeshes = submeshes.size() - submeshStart;
    const auto requestedSubmeshes = mesh_instance.submesh_count == std::numeric_limits<uint32_t>::max()
                                        ? availableSubmeshes
                                        : static_cast<size_t>(mesh_instance.submesh_count);
    const auto submeshEnd = submeshStart + std::min(availableSubmeshes, requestedSubmeshes);

    for (size_t submeshIndex = submeshStart; submeshIndex < submeshEnd; ++submeshIndex) {
        const auto& submesh = submeshes[submeshIndex];
        const interface::Material* material = nullptr;
        if (submesh.material_slot < mesh_instance.materials.size()) {
            const auto materialHandle = mesh_instance.materials[submesh.material_slot];
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
            drawSubMesh(*mesh, submeshIndex, submesh, *material, mesh_instance.transform);
        }
    }
}

void GeometryPass::drawSubMesh(const interface::Mesh& mesh,
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
    const auto& materialGpu = m_material_cache->getOrCreate(parameters);
    m_object_uniforms.model = transform;
    m_object_uniforms.normalMatrix = glm::transpose(glm::inverse(transform));
    m_object_uniforms.materialAlbedo = parameters.albedo;
    m_object_uniforms.materialEmission = parameters.emission;
    m_object_uniforms.materialEmissionStrength = parameters.emission_strength;
    m_object_uniforms.materialMetallic = parameters.metallic;
    m_object_uniforms.materialRoughness = parameters.roughness;
    m_object_uniforms.materialShadingModel = parameters.shading_model == interface::ShadingModel::Unlit ? 1u : 0u;
    m_object_uniforms.materialNormalScale = std::max(parameters.normal_scale, 0.0f);
    m_object_uniforms.materialOcclusionStrength = parameters.occlusion_strength;
    m_object_uniforms.materialHasNormalMap = parameters.normal_texture.isValid() ? 1u : 0u;
    m_device->updateBuffer(m_object_uniform_buffer, 0, &m_object_uniforms, sizeof(ObjectUniforms));

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

void GeometryPass::flushFrameUniforms()
{
    if (!*m_frame_uniforms_dirty) {
        return;
    }

    m_device->updateBuffer(m_frame_uniform_buffer, 0, m_frame_uniforms, sizeof(FrameUniforms));
    *m_frame_uniforms_dirty = false;
}

MeshGpuData* GeometryPass::getOrCreateSubMeshGpuData(const interface::Mesh& mesh,
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
        LUNA_ASSERT(gpu_mesh.vertex_buffer, "Failed to create mesh vertex buffer.");
        gpu_mesh.uploaded_vertex_version = 0;
        LUNA_CORE_DEBUG("{} mesh vertex buffer for asset {} submesh {} (vertices: {}, capacity: {} bytes)",
                        creatingVertexBuffer ? "Created" : "Recreated",
                        mesh.handle.toString(),
                        submesh_index,
                        vertices.size(),
                        gpu_mesh.vertex_buffer_capacity);
    }

    if (gpu_mesh.uploaded_vertex_version != vertex_version) {
        m_device->updateBuffer(gpu_mesh.vertex_buffer, 0, vertices.data(), vertex_buffer_size);
        gpu_mesh.uploaded_vertex_version = vertex_version;
        LUNA_CORE_DEBUG("Uploaded mesh vertices for asset {} submesh {} (vertices: {}, version: {})",
                        mesh.handle.toString(),
                        submesh_index,
                        vertices.size(),
                        vertex_version);
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
        LUNA_ASSERT(gpu_mesh.index_buffer, "Failed to create mesh index buffer.");
        gpu_mesh.uploaded_index_version = 0;
        LUNA_CORE_DEBUG("{} mesh index buffer for asset {} submesh {} (indices: {}, capacity: {} bytes)",
                        creatingIndexBuffer ? "Created" : "Recreated",
                        mesh.handle.toString(),
                        submesh_index,
                        indices.size(),
                        gpu_mesh.index_buffer_capacity);
    }

    if (!indices.empty() && gpu_mesh.uploaded_index_version != index_version) {
        m_device->updateBuffer(gpu_mesh.index_buffer, 0, indices.data(), index_buffer_size);
        gpu_mesh.uploaded_index_version = index_version;
        LUNA_CORE_DEBUG("Uploaded mesh indices for asset {} submesh {} (indices: {}, version: {})",
                        mesh.handle.toString(),
                        submesh_index,
                        indices.size(),
                        index_version);
    } else if (indices.empty() && gpu_mesh.index_buffer) {
        m_device->destroyBuffer(gpu_mesh.index_buffer);
        gpu_mesh.index_buffer = {};
        gpu_mesh.index_buffer_dynamic = false;
        gpu_mesh.index_buffer_capacity = 0;
        gpu_mesh.uploaded_index_version = 0;
        LUNA_CORE_DEBUG("Destroyed mesh index buffer for asset {} submesh {} because it has no indices",
                        mesh.handle.toString(),
                        submesh_index);
    }

    gpu_mesh.vertex_count = static_cast<uint32_t>(vertices.size());
    gpu_mesh.index_count = static_cast<uint32_t>(indices.size());
    return &gpu_mesh;
}

uint64_t GeometryPass::getSubMeshCacheKey(const interface::Mesh& mesh, size_t submesh_index) const
{
    const auto handle = static_cast<uint64_t>(mesh.handle);
    LUNA_ASSERT(handle != 0);

    return handle ^ ((static_cast<uint64_t>(submesh_index) + 1u) * 0x9E'37'79'B9'7F'4A'7C'15ull);
}

} // namespace lunalite::renderer
