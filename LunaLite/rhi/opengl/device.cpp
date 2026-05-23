#include "command_list.h"
#include "device.h"

namespace lunalite::rhi {

OpenGLDevice::OpenGLDevice()
    : m_command_list(std::make_unique<OpenGLCommandList>(*this))
{}

OpenGLDevice::~OpenGLDevice()
{
    m_command_list.reset();

    for (auto& pipeline : m_pipelines) {
        glDeleteVertexArrays(1, &pipeline.vao);
        glDeleteProgram(pipeline.program);
    }

    for (auto& shader : m_shaders) {
        glDeleteShader(shader.id);
    }

    for (auto& buffer : m_buffers) {
        glDeleteBuffers(1, &buffer.id);
    }

    for (auto& texture : m_textures) {
        if (texture.id != 0 && !texture.is_swapchain_backbuffer) {
            glDeleteTextures(1, &texture.id);
        }
    }
}

CommandList& OpenGLDevice::getCommandList()
{
    return *m_command_list;
}

OpenGLBuffer* OpenGLDevice::getBuffer(BufferHandle handle)
{
    if (handle == 0 || handle > m_buffers.size()) {
        return nullptr;
    }

    auto& buffer = m_buffers[handle - 1];
    if (buffer.id == 0) {
        return nullptr;
    }

    return &m_buffers[handle - 1];
}

OpenGLTexture* OpenGLDevice::getTexture(TextureHandle handle)
{
    if (handle == 0 || handle > m_textures.size()) {
        return nullptr;
    }

    auto& texture = m_textures[handle - 1];
    if (texture.id == 0 && !texture.is_swapchain_backbuffer) {
        return nullptr;
    }

    return &texture;
}

OpenGLTextureView* OpenGLDevice::getTextureView(TextureViewHandle handle)
{
    if (handle == 0 || handle > m_texture_views.size()) {
        return nullptr;
    }

    auto& view = m_texture_views[handle - 1];
    if (view.texture == 0) {
        return nullptr;
    }

    return &view;
}

OpenGLShader* OpenGLDevice::getShader(ShaderHandle handle)
{
    if (handle == 0 || handle > m_shaders.size()) {
        return nullptr;
    }

    auto& shader = m_shaders[handle - 1];
    if (shader.id == 0) {
        return nullptr;
    }

    return &shader;
}

OpenGLPipeline* OpenGLDevice::getPipeline(PipelineHandle handle)
{
    if (handle == 0 || handle > m_pipelines.size()) {
        return nullptr;
    }

    auto& pipeline = m_pipelines[handle - 1];
    if (pipeline.program == 0 || pipeline.vao == 0) {
        return nullptr;
    }

    return &pipeline;
}

} // namespace lunalite::rhi
