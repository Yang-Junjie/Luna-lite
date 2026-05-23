
#pragma once
#include "buffer.h"
#include "command_list.h"
#include "pipeline.h"
#include "rhi_types.h"
#include "shader.h"
#include "texture.h"

namespace lunalite::rhi {
class Device {
public:
    virtual ~Device() = default;
    virtual BufferHandle createBuffer(const BufferDesc& desc, const void* data) = 0;
    virtual void updateBuffer(BufferHandle buffer, const void* data, size_t size) = 0;
    virtual void destroyBuffer(BufferHandle buffer) = 0;

    virtual TextureHandle createTexture(const TextureDesc& desc) = 0;
    virtual void destroyTexture(TextureHandle texture) = 0;

    virtual TextureViewHandle createTextureView(const TextureViewDesc& desc) = 0;
    virtual void destroyTextureView(TextureViewHandle view) = 0;

    virtual ShaderHandle createShader(const ShaderDesc& desc) = 0;
    virtual void destroyShader(ShaderHandle shader) = 0;

    virtual PipelineHandle createPipeline(const PipelineDesc& desc) = 0;
    virtual void destroyPipeline(PipelineHandle pipeline) = 0;

    virtual CommandList& getCommandList() = 0;
};
} // namespace lunalite::rhi
