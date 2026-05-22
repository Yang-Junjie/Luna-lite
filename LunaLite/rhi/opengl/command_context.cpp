#include "command_context.h"
#include "device.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace lunalite::rhi {

OpenGLCommandContext::OpenGLCommandContext(OpenGLDevice& device, void* native_window)
    : m_device(device),
      m_native_window(native_window)
{}

void OpenGLCommandContext::beginFrame() {}

void OpenGLCommandContext::clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLCommandContext::bindPipeline(PipelineHandle pipeline)
{
    auto* glPipeline = m_device.getPipeline(pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    m_current_pipeline = pipeline;
    glUseProgram(glPipeline->program);
    glBindVertexArray(glPipeline->vao);
}

void OpenGLCommandContext::bindVertexBuffer(BufferHandle buffer)
{
    auto* glBuffer = m_device.getBuffer(buffer);
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);

    if (glBuffer == nullptr || glPipeline == nullptr) {
        return;
    }

    glVertexArrayVertexBuffer(glPipeline->vao, 0, glBuffer->id, 0, glPipeline->vertex_layout.stride);
}

void OpenGLCommandContext::draw(uint32_t vertex_count, uint32_t first_vertex)
{
    auto* glPipeline = m_device.getPipeline(m_current_pipeline);
    if (glPipeline == nullptr) {
        return;
    }

    glDrawArrays(glPipeline->topology, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
}

void OpenGLCommandContext::endFrame() {}

void OpenGLCommandContext::present()
{
    auto* glfw_window = static_cast<GLFWwindow*>(m_native_window);
    if (glfw_window == nullptr) {
        return;
    }

    glfwSwapBuffers(glfw_window);
}

} // namespace lunalite::rhi
