#include "../../../core/log.h"
#include "debug_line_pass.h"

#include <algorithm>
#include <limits>

namespace lunalite::renderer {

DebugLinePass::DebugLinePass(rhi::Device& device,
                             rhi::CommandList& commands,
                             rhi::PipelineHandle line_depth_pipeline,
                             rhi::PipelineHandle line_overlay_pipeline,
                             rhi::BindGroupHandle geometry_bind_group,
                             rhi::BufferHandle frame_uniform_buffer,
                             rhi::BufferHandle object_uniform_buffer,
                             FrameUniforms& frame_uniforms,
                             bool& frame_uniforms_dirty)
    : m_device(&device),
      m_cmd(&commands),
      m_line_depth_pipeline(line_depth_pipeline),
      m_line_overlay_pipeline(line_overlay_pipeline),
      m_geometry_bind_group(geometry_bind_group),
      m_frame_uniform_buffer(frame_uniform_buffer),
      m_object_uniform_buffer(object_uniform_buffer),
      m_frame_uniforms(&frame_uniforms),
      m_frame_uniforms_dirty(&frame_uniforms_dirty)
{
    m_line_vertex_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(LineVertex) * 2,
        .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::VertexBuffer,
    });
    m_line_vertex_buffer_capacity = sizeof(LineVertex) * 2;
    LUNA_ASSERT(m_line_vertex_buffer, "Failed to create line vertex buffer.");
}

DebugLinePass::~DebugLinePass()
{
    if (m_line_vertex_buffer) {
        m_device->destroyBuffer(m_line_vertex_buffer);
        m_line_vertex_buffer = {};
    }
}

uint32_t DebugLinePass::execute(const GBuffer& gbuffer, std::span<const interface::LineDrawCommand> line_commands)
{
    if (line_commands.empty() || gbuffer.width == 0 || gbuffer.height == 0 || !gbuffer.final_color_view) {
        return 0;
    }

    m_depth_test_vertices.clear();
    m_overlay_vertices.clear();
    m_depth_test_vertices.reserve(line_commands.size() * 2);
    m_overlay_vertices.reserve(line_commands.size() * 2);

    for (const auto& lineCommand : line_commands) {
        auto& vertices = lineCommand.depth_test ? m_depth_test_vertices : m_overlay_vertices;
        const auto color = glm::vec3{lineCommand.color};
        vertices.push_back(LineVertex{.position = lineCommand.start, .color = color});
        vertices.push_back(LineVertex{.position = lineCommand.end, .color = color});
    }

    flushFrameUniforms();

    m_object_uniforms.model = glm::mat4{1.0f};
    m_object_uniforms.normalMatrix = glm::mat4{1.0f};
    m_device->updateBuffer(m_object_uniform_buffer, 0, &m_object_uniforms, sizeof(ObjectUniforms));

    uint32_t drawCalls = 0;
    drawCalls += renderBatch(gbuffer, m_line_depth_pipeline, m_depth_test_vertices, true);
    drawCalls += renderBatch(gbuffer, m_line_overlay_pipeline, m_overlay_vertices, false);
    return drawCalls;
}

void DebugLinePass::flushFrameUniforms()
{
    if (!*m_frame_uniforms_dirty) {
        return;
    }

    m_device->updateBuffer(m_frame_uniform_buffer, 0, m_frame_uniforms, sizeof(FrameUniforms));
    *m_frame_uniforms_dirty = false;
}

void DebugLinePass::ensureVertexBuffer(size_t vertex_count)
{
    const auto requiredSize = vertex_count * sizeof(LineVertex);
    if (requiredSize == 0 || m_line_vertex_buffer_capacity >= requiredSize) {
        return;
    }

    if (m_line_vertex_buffer) {
        m_device->destroyBuffer(m_line_vertex_buffer);
        m_line_vertex_buffer = {};
    }

    m_line_vertex_buffer_capacity = growBufferCapacity(m_line_vertex_buffer_capacity, requiredSize);
    m_line_vertex_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = m_line_vertex_buffer_capacity,
        .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::VertexBuffer,
    });
    LUNA_ASSERT(m_line_vertex_buffer, "Failed to grow line vertex buffer.");
}

uint32_t DebugLinePass::renderBatch(const GBuffer& gbuffer,
                                    rhi::PipelineHandle pipeline,
                                    std::span<const LineVertex> vertices,
                                    bool depth_test)
{
    if (vertices.empty() || !pipeline) {
        return 0;
    }
    LUNA_ASSERT(vertices.size() <= std::numeric_limits<uint32_t>::max(), "Too many debug line vertices.");

    ensureVertexBuffer(vertices.size());
    m_device->updateBuffer(m_line_vertex_buffer, 0, vertices.data(), vertices.size() * sizeof(LineVertex));

    const rhi::TextureTransition colorBarrier{
        .texture = gbuffer.final_color_texture,
        .state = rhi::ResourceState::ColorAttachment,
    };
    m_cmd->transition(&colorBarrier, 1);

    if (depth_test && gbuffer.depth_texture) {
        const rhi::TextureTransition depthBarrier{
            .texture = gbuffer.depth_texture,
            .state = rhi::ResourceState::DepthStencilWrite,
        };
        m_cmd->transition(&depthBarrier, 1);
    }

    rhi::RenderPassBeginInfo pass;
    pass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = gbuffer.final_color_view,
        .load_op = rhi::LoadOp::Load,
        .store_op = rhi::StoreOp::Store,
    });
    if (depth_test && gbuffer.depth_view) {
        pass.has_depth_stencil_attachment = true;
        pass.depth_stencil_attachment = rhi::DepthStencilAttachmentDesc{
            .view = gbuffer.depth_view,
            .depth_load_op = rhi::LoadOp::Load,
            .depth_store_op = rhi::StoreOp::Store,
        };
    }
    pass.width = gbuffer.width;
    pass.height = gbuffer.height;

    m_cmd->beginRenderPass(pass);
    m_cmd->setPipeline(pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);
    m_cmd->setVertexBuffer(0, m_line_vertex_buffer);
    m_cmd->draw(static_cast<uint32_t>(vertices.size()));
    m_cmd->endRenderPass();
    return 1;
}

} // namespace lunalite::renderer
