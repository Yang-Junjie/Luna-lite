#pragma once
#include "../interface/device.h"
#include "../interface/rhi_types.h"

#include <cstddef>

#include <glad/glad.h>
#include <memory>
#include <vector>

namespace lunalite::rhi {

class OpenGLCommandList;

struct OpenGLBuffer {
    GLuint id{0};
    BufferType type{BufferType::VertexBuffer};
    size_t size{0};
};

struct OpenGLShader {
    GLuint id{0};
    ShaderStage stage{ShaderStage::Vertex};
};

struct OpenGLPipeline {
    GLuint program{0};
    GLuint vao{0};
    GLenum topology{GL_TRIANGLES};
    VertexLayoutDesc vertex_layout;
};

class OpenGLDevice final : public Device {
public:
    OpenGLDevice();
    ~OpenGLDevice() override;

    BufferHandle createBuffer(const BufferDesc& desc, const void* data) override;
    void updateBuffer(BufferHandle buffer, const void* data, size_t size) override;
    void destroyBuffer(BufferHandle buffer) override;

    ShaderHandle createShader(const ShaderDesc& desc) override;
    void destroyShader(ShaderHandle shader) override;

    PipelineHandle createPipeline(const PipelineDesc& desc) override;
    void destroyPipeline(PipelineHandle pipeline) override;

    CommandList& getCommandList() override;

    OpenGLBuffer* getBuffer(BufferHandle handle);
    OpenGLShader* getShader(ShaderHandle handle);
    OpenGLPipeline* getPipeline(PipelineHandle handle);

private:
    std::vector<OpenGLBuffer> m_buffers;
    std::vector<OpenGLShader> m_shaders;
    std::vector<OpenGLPipeline> m_pipelines;
    std::unique_ptr<OpenGLCommandList> m_command_list;
};

} // namespace lunalite::rhi
