#include "command_context.h"
#include "device.h"

#include <cstdio>

namespace lunalite::rhi {
namespace {

GLenum toGLBufferUsage(BufferUsage usage)
{
    switch (usage) {
        case BufferUsage::Static:
            return GL_STATIC_DRAW;
        case BufferUsage::Dynamic:
            return GL_DYNAMIC_DRAW;
    }

    return GL_STATIC_DRAW;
}

GLenum toGLShaderStage(ShaderStage stage)
{
    switch (stage) {
        case ShaderStage::Vertex:
            return GL_VERTEX_SHADER;
        case ShaderStage::Fragment:
            return GL_FRAGMENT_SHADER;
    }

    return GL_VERTEX_SHADER;
}

GLenum toGLTopology(PrimitiveTopology topology)
{
    switch (topology) {
        case PrimitiveTopology::Triangle:
            return GL_TRIANGLES;
        case PrimitiveTopology::Line:
            return GL_LINES;
        case PrimitiveTopology::Point:
            return GL_POINTS;
    }

    return GL_TRIANGLES;
}

uint32_t vertexFormatComponentCount(VertexFormat format)
{
    switch (format) {
        case VertexFormat::Float1:
        case VertexFormat::Int1:
        case VertexFormat::UInt1:
        case VertexFormat::Bool1:
        case VertexFormat::Byte:
            return 1;
        case VertexFormat::Float2:
        case VertexFormat::Int2:
        case VertexFormat::UInt2:
        case VertexFormat::Bool2:
            return 2;
        case VertexFormat::Float3:
        case VertexFormat::Int3:
        case VertexFormat::UInt3:
        case VertexFormat::Bool3:
            return 3;
        case VertexFormat::Float4:
        case VertexFormat::Int4:
        case VertexFormat::UInt4:
        case VertexFormat::Bool4:
            return 4;
    }

    return 1;
}

GLenum vertexFormatType(VertexFormat format)
{
    switch (format) {
        case VertexFormat::Float1:
        case VertexFormat::Float2:
        case VertexFormat::Float3:
        case VertexFormat::Float4:
            return GL_FLOAT;
        case VertexFormat::Int1:
        case VertexFormat::Int2:
        case VertexFormat::Int3:
        case VertexFormat::Int4:
            return GL_INT;
        case VertexFormat::UInt1:
        case VertexFormat::UInt2:
        case VertexFormat::UInt3:
        case VertexFormat::UInt4:
            return GL_UNSIGNED_INT;
        case VertexFormat::Bool1:
        case VertexFormat::Bool2:
        case VertexFormat::Bool3:
        case VertexFormat::Bool4:
            return GL_BOOL;
        case VertexFormat::Byte:
            return GL_BYTE;
    }

    return GL_FLOAT;
}

bool isIntegerVertexFormat(VertexFormat format)
{
    switch (format) {
        case VertexFormat::Int1:
        case VertexFormat::Int2:
        case VertexFormat::Int3:
        case VertexFormat::Int4:
        case VertexFormat::UInt1:
        case VertexFormat::UInt2:
        case VertexFormat::UInt3:
        case VertexFormat::UInt4:
        case VertexFormat::Bool1:
        case VertexFormat::Bool2:
        case VertexFormat::Bool3:
        case VertexFormat::Bool4:
        case VertexFormat::Byte:
            return true;
        case VertexFormat::Float1:
        case VertexFormat::Float2:
        case VertexFormat::Float3:
        case VertexFormat::Float4:
            return false;
    }

    return false;
}

GLuint vertexAttributeLocation(VertexAttribute semantic)
{
    switch (semantic) {
        case VertexAttribute::Position:
            return 0;
        case VertexAttribute::Normal:
            return 1;
        case VertexAttribute::TexCoord:
            return 2;
        case VertexAttribute::Color:
            return 3;
    }

    return 0;
}

void logShaderError(GLuint shader)
{
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1) {
        return;
    }

    std::vector<GLchar> log(static_cast<size_t>(logLength));
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::printf("OpenGL shader compile failed:\n%s\n", log.data());
}

void logProgramError(GLuint program)
{
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1) {
        return;
    }

    std::vector<GLchar> log(static_cast<size_t>(logLength));
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    std::printf("OpenGL program link failed:\n%s\n", log.data());
}

} // namespace

OpenGLDevice::OpenGLDevice(void* nativeWindow)
    : m_context(std::make_unique<OpenGLCommandContext>(*this, nativeWindow))
{}

OpenGLDevice::~OpenGLDevice()
{
    m_context.reset();

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
}

BufferHandle OpenGLDevice::createBuffer(const BufferDesc& desc, const void* data)
{
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    glNamedBufferData(buffer, static_cast<GLsizeiptr>(desc.size), data, toGLBufferUsage(desc.usage));

    m_buffers.push_back(OpenGLBuffer{.id = buffer, .type = desc.type, .size = desc.size});
    return static_cast<BufferHandle>(m_buffers.size());
}

ShaderHandle OpenGLDevice::createShader(const ShaderDesc& desc)
{
    GLuint shader = glCreateShader(toGLShaderStage(desc.stage));
    glShaderSource(shader, 1, &desc.source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        logShaderError(shader);
        glDeleteShader(shader);
        return 0;
    }

    m_shaders.push_back(OpenGLShader{.id = shader, .stage = desc.stage});
    return static_cast<ShaderHandle>(m_shaders.size());
}

PipelineHandle OpenGLDevice::createPipeline(const PipelineDesc& desc)
{
    const auto* vertexShader = getShader(desc.vertex_shader);
    const auto* fragmentShader = getShader(desc.fragment_shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader->id);
    glAttachShader(program, fragmentShader->id);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        logProgramError(program);
        glDeleteProgram(program);
        return 0;
    }

    GLuint vao = 0;
    glCreateVertexArrays(1, &vao);

    for (const auto& attribute : desc.vertex_layout.attributes) {
        const GLuint location = vertexAttributeLocation(attribute.semantic);
        const auto componentCount = static_cast<GLint>(vertexFormatComponentCount(attribute.format));
        const GLenum type = vertexFormatType(attribute.format);

        glEnableVertexArrayAttrib(vao, location);

        if (isIntegerVertexFormat(attribute.format)) {
            glVertexArrayAttribIFormat(vao, location, componentCount, type, attribute.offset);
        } else {
            glVertexArrayAttribFormat(vao, location, componentCount, type, GL_FALSE, attribute.offset);
        }

        glVertexArrayAttribBinding(vao, location, 0);
    }

    m_pipelines.push_back(OpenGLPipeline{
        .program = program, .vao = vao, .topology = toGLTopology(desc.topology), .vertex_layout = desc.vertex_layout});
    return static_cast<PipelineHandle>(m_pipelines.size());
}

CommandContext& OpenGLDevice::getImmediateCmdContext()
{
    return *m_context;
}

OpenGLBuffer* OpenGLDevice::getBuffer(BufferHandle handle)
{
    return &m_buffers[handle - 1];
}

OpenGLShader* OpenGLDevice::getShader(ShaderHandle handle)
{
    return &m_shaders[handle - 1];
}

OpenGLPipeline* OpenGLDevice::getPipeline(PipelineHandle handle)
{
    return &m_pipelines[handle - 1];
}

} // namespace lunalite::rhi
