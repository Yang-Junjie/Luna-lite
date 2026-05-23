#include "command_list.h"
#include "device.h"
#include "gl_convert.h"

#include <cstdio>
#include <cstdint>

#include <glad/glad.h>
#include <vector>

namespace lunalite::rhi {

OpenGLCommandList::OpenGLCommandList(OpenGLDevice& device)
    : m_device(device)
{}

namespace {
size_t indexFormatSize(IndexFormat format)
{
    switch (format) {
        case IndexFormat::UInt16:
            return sizeof(uint16_t);
        case IndexFormat::UInt32:
            return sizeof(uint32_t);
    }

    return sizeof(uint32_t);
}
} // namespace

void OpenGLCommandList::begin()
{
    m_current_pipeline = 0;
    m_current_index_buffer = 0;
    m_current_index_format = IndexFormat::UInt32;
}

void OpenGLCommandList::end() {}

void OpenGLCommandList::beginRenderPass(const RenderPassBeginInfo& info)
{
    if (info.color_attachments.empty() && !info.has_depth_stencil_attachment) {
        std::printf("OpenGL render pass begin failed: no attachments.\n");
        return;
    }

    bool uses_swapchain = false;
    bool uses_offscreen_texture = false;
    GLuint framebuffer = 0;

    const auto inspectAttachment = [&](TextureViewHandle view, const char* label) {
        const auto* glView = m_device.getTextureView(view);
        const auto* glTexture = glView ? m_device.getTexture(glView->texture) : nullptr;
        if (glTexture == nullptr) {
            std::printf("OpenGL render pass begin failed: invalid %s attachment.\n", label);
            return false;
        }

        if (glTexture->is_swapchain_backbuffer) {
            uses_swapchain = true;
        } else {
            uses_offscreen_texture = true;
        }

        return true;
    };

    for (const auto& color : info.color_attachments) {
        if (!inspectAttachment(color.view, "color")) {
            return;
        }
    }

    if (info.has_depth_stencil_attachment && !inspectAttachment(info.depth_stencil_attachment.view, "depth stencil")) {
        return;
    }

    if (uses_swapchain && uses_offscreen_texture) {
        std::printf("OpenGL render pass begin failed: cannot mix swapchain and offscreen attachments.\n");
        return;
    }

    if (!uses_swapchain) {
        glCreateFramebuffers(1, &framebuffer);
        m_render_pass_fbo = framebuffer;

        std::vector<GLenum> drawBuffers;
        drawBuffers.reserve(info.color_attachments.size());

        for (size_t i = 0; i < info.color_attachments.size(); ++i) {
            const auto* colorView = m_device.getTextureView(info.color_attachments[i].view);
            const auto* colorTexture = colorView ? m_device.getTexture(colorView->texture) : nullptr;
            if (colorTexture != nullptr) {
                const auto attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
                glNamedFramebufferTexture(framebuffer, attachment, colorTexture->id, 0);
                drawBuffers.push_back(attachment);
            }
        }

        if (drawBuffers.empty()) {
            glNamedFramebufferDrawBuffer(framebuffer, GL_NONE);
            glNamedFramebufferReadBuffer(framebuffer, GL_NONE);
        } else {
            glNamedFramebufferDrawBuffers(framebuffer, static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
            glNamedFramebufferReadBuffer(framebuffer, drawBuffers.front());
        }

        if (info.has_depth_stencil_attachment) {
            const auto* depthView = m_device.getTextureView(info.depth_stencil_attachment.view);
            const auto* depthTexture = depthView ? m_device.getTexture(depthView->texture) : nullptr;
            if (depthTexture != nullptr) {
                glNamedFramebufferTexture(framebuffer, toGLAttachment(depthView->format), depthTexture->id, 0);
            }
        }

        if (glCheckNamedFramebufferStatus(framebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::printf("OpenGL render pass begin failed: framebuffer is incomplete.\n");
            glDeleteFramebuffers(1, &framebuffer);
            m_render_pass_fbo = 0;
            return;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    if (uses_swapchain) {
        if (info.color_attachments.empty()) {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        } else {
            glDrawBuffer(GL_BACK);
            glReadBuffer(GL_BACK);
        }
    }

    if (info.width > 0 && info.height > 0) {
        glViewport(0, 0, static_cast<GLsizei>(info.width), static_cast<GLsizei>(info.height));
    }

    for (size_t i = 0; i < info.color_attachments.size(); ++i) {
        const auto& color = info.color_attachments[i];
        if (color.load_op == LoadOp::Clear) {
            const float clearColor[] = {
                color.clear_color.r,
                color.clear_color.g,
                color.clear_color.b,
                color.clear_color.a,
            };
            glClearBufferfv(GL_COLOR, static_cast<GLint>(i), clearColor);
        }
    }

    if (info.has_depth_stencil_attachment && info.depth_stencil_attachment.depth_load_op == LoadOp::Clear) {
        const float clearDepth = info.depth_stencil_attachment.clear_depth;
        glClearBufferfv(GL_DEPTH, 0, &clearDepth);
    }
}

void OpenGLCommandList::endRenderPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (m_render_pass_fbo != 0) {
        glDeleteFramebuffers(1, &m_render_pass_fbo);
        m_render_pass_fbo = 0;
    }
}

void OpenGLCommandList::setPipeline(PipelineHandle pipeline)
{
    auto* glPipeline = m_device.getPipeline(pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    m_current_pipeline = pipeline;
    glUseProgram(glPipeline->program);
    glBindVertexArray(glPipeline->vao);

    if (glPipeline->depth_state.enabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(glPipeline->depth_state.write_enabled ? GL_TRUE : GL_FALSE);
        glDepthFunc(toGLCompareOp(glPipeline->depth_state.compare));
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    if (glPipeline->raster_state.cull_mode == CullMode::None) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(toGLCullMode(glPipeline->raster_state.cull_mode));
    }
    glFrontFace(toGLFrontFace(glPipeline->raster_state.front_face));

    if (glPipeline->blend_state.enabled) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(toGLBlendFactor(glPipeline->blend_state.src_color),
                            toGLBlendFactor(glPipeline->blend_state.dst_color),
                            toGLBlendFactor(glPipeline->blend_state.src_alpha),
                            toGLBlendFactor(glPipeline->blend_state.dst_alpha));
        glBlendEquationSeparate(toGLBlendOp(glPipeline->blend_state.color_op),
                                toGLBlendOp(glPipeline->blend_state.alpha_op));
    } else {
        glDisable(GL_BLEND);
    }
}

void OpenGLCommandList::setVertexBuffer(uint32_t slot, BufferHandle buffer)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);

    if (glBuffer == nullptr || glPipeline == nullptr || glBuffer->type != BufferType::VertexBuffer) {
        return;
    }

    glVertexArrayVertexBuffer(glPipeline->vao, slot, glBuffer->id, 0, glPipeline->vertex_layout.stride);
}

void OpenGLCommandList::setIndexBuffer(BufferHandle buffer, IndexFormat format)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);

    if (glBuffer == nullptr || glPipeline == nullptr || glBuffer->type != BufferType::IndexBuffer) {
        return;
    }

    m_current_index_buffer = buffer;
    m_current_index_format = format;
    glVertexArrayElementBuffer(glPipeline->vao, glBuffer->id);
}

void OpenGLCommandList::setUniformBuffer(uint32_t binding, BufferHandle buffer)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    if (glBuffer == nullptr || glBuffer->type != BufferType::UniformBuffer) {
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

void OpenGLCommandList::drawIndexed(uint32_t index_count, uint32_t first_index, int32_t vertex_offset)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    auto* glIndexBuffer = m_device.getBuffer(m_current_index_buffer);
    if (glPipeline == nullptr || glIndexBuffer == nullptr || glIndexBuffer->type != BufferType::IndexBuffer) {
        return;
    }

    const auto offset = static_cast<uintptr_t>(first_index) * indexFormatSize(m_current_index_format);
    glDrawElementsBaseVertex(glPipeline->topology,
                             static_cast<GLsizei>(index_count),
                             toGLIndexFormat(m_current_index_format),
                             reinterpret_cast<const void*>(offset),
                             static_cast<GLint>(vertex_offset));
}

} // namespace lunalite::rhi
