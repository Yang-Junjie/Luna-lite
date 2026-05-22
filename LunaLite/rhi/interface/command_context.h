#pragma once
#include "rhi_types.h"

namespace lunalite::rhi {

class CommandContext {
public:
    virtual ~CommandContext() = default;
    virtual void beginFrame() = 0;
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void bindPipeline(PipelineHandle pipeline) = 0;
    virtual void bindVertexBuffer(BufferHandle buffer) = 0;
    virtual void bindUniformBuffer(BufferHandle buffer, uint32_t binding) = 0;
    virtual void draw(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;
};
} // namespace lunalite::rhi
