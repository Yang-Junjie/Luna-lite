#include "../../../core/log.h"
#include "../../interface/mesh.h"
#include "debug_line_pass.h"

namespace lunalite::renderer {

DebugLinePass::DebugLinePass(rhi::Device& device,
                             rhi::CommandList& commands,
                             rhi::PipelineHandle line_pipeline,
                             rhi::PipelineHandle geometry_pipeline,
                             rhi::BindGroupHandle geometry_bind_group,
                             rhi::BufferHandle frame_uniform_buffer,
                             rhi::BufferHandle object_uniform_buffer,
                             FrameUniforms& frame_uniforms,
                             bool& frame_uniforms_dirty)
    : m_device(&device),
      m_cmd(&commands),
      m_line_pipeline(line_pipeline),
      m_geometry_pipeline(geometry_pipeline),
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
    LUNA_ASSERT(m_line_vertex_buffer, "Failed to create line vertex buffer.");
}

DebugLinePass::~DebugLinePass()
{
    if (m_line_vertex_buffer) {
        m_device->destroyBuffer(m_line_vertex_buffer);
        m_line_vertex_buffer = {};
    }
}

void DebugLinePass::renderLine(const interface::LineDrawCommand& line_command)
{
    const LineVertex vertices[] = {
        LineVertex{.position = line_command.start, .color = glm::vec3{line_command.color}},
        LineVertex{.position = line_command.end, .color = glm::vec3{line_command.color}},
    };

    flushFrameUniforms();

    m_object_uniforms.model = glm::mat4{1.0f};
    m_object_uniforms.normalMatrix = glm::mat4{1.0f};
    m_device->updateBuffer(m_object_uniform_buffer, 0, &m_object_uniforms, sizeof(ObjectUniforms));
    m_device->updateBuffer(m_line_vertex_buffer, 0, vertices, sizeof(vertices));

    m_cmd->setPipeline(m_line_pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);
    m_cmd->setVertexBuffer(0, m_line_vertex_buffer);
    m_cmd->draw(2);
    m_cmd->setPipeline(m_geometry_pipeline);
}

void DebugLinePass::flushFrameUniforms()
{
    if (!*m_frame_uniforms_dirty) {
        return;
    }

    m_device->updateBuffer(m_frame_uniform_buffer, 0, m_frame_uniforms, sizeof(FrameUniforms));
    *m_frame_uniforms_dirty = false;
}

} // namespace lunalite::renderer
