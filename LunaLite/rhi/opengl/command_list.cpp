#include "command_list.h"
#include "device.h"

#include <glad/glad.h>

namespace lunalite::rhi {

OpenGLCommandList::OpenGLCommandList(OpenGLDevice& device)
    : m_device(device)
{}

void OpenGLCommandList::begin() {}

void OpenGLCommandList::end() {}

void OpenGLCommandList::beginRenderPass(const RenderPassBeginInfo& info)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (info.width > 0 && info.height > 0) {
        glViewport(0, 0, static_cast<GLsizei>(info.width), static_cast<GLsizei>(info.height));
    }

    GLbitfield clearMask = 0;
    if (!info.color_attachments.empty()) {
        const auto& color = info.color_attachments.front();
        if (color.load_op == LoadOp::Clear) {
            glClearColor(color.clear_color.r, color.clear_color.g, color.clear_color.b, color.clear_color.a);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
    }

    if (info.has_depth_stencil_attachment && info.depth_stencil_attachment.depth_load_op == LoadOp::Clear) {
        glClearDepth(info.depth_stencil_attachment.clear_depth);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (clearMask != 0) {
        glClear(clearMask);
    }
}

void OpenGLCommandList::endRenderPass() {}

void OpenGLCommandList::setPipeline(PipelineHandle pipeline)
{
    auto* glPipeline = m_device.getPipeline(pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    m_current_pipeline = pipeline;
    glUseProgram(glPipeline->program);
    glBindVertexArray(glPipeline->vao);
}

void OpenGLCommandList::setVertexBuffer(uint32_t slot, BufferHandle buffer)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);

    if (glBuffer == nullptr || glPipeline == nullptr) {
        return;
    }

    glVertexArrayVertexBuffer(glPipeline->vao, slot, glBuffer->id, 0, glPipeline->vertex_layout.stride);
}

void OpenGLCommandList::setUniformBuffer(uint32_t binding, BufferHandle buffer)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    if (glBuffer == nullptr) {
        return;
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, binding, glBuffer->id);
}

void OpenGLCommandList::draw(uint32_t vertex_count, uint32_t first_vertex)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    glDrawArrays(glPipeline->topology, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
}

} // namespace lunalite::rhi
