#pragma once
#include "../interface/buffer.h"
#include "../interface/pipeline.h"
#include "../interface/shader.h"
#include "../interface/texture.h"

#include <cstdint>

#include <glad/glad.h>

namespace lunalite::rhi {

GLenum toGLBufferUsage(BufferUsage usage);
GLenum toGLShaderStage(ShaderStage stage);
GLenum toGLTopology(PrimitiveTopology topology);
GLenum toGLTextureInternalFormat(TextureFormat format);
GLenum toGLAttachment(TextureFormat format);
bool isDepthFormat(TextureFormat format);
uint32_t vertexFormatComponentCount(VertexFormat format);
GLenum vertexFormatType(VertexFormat format);
bool isIntegerVertexFormat(VertexFormat format);
GLuint vertexAttributeLocation(VertexAttribute semantic);

} // namespace lunalite::rhi
