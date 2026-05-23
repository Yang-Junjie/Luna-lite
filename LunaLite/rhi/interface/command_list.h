#pragma once
#include "rhi_types.h"

namespace lunalite::rhi {

class CommandList {
public:
    virtual ~CommandList() = default;

    virtual void begin() = 0;
    virtual void end() = 0;

    virtual void beginRenderPass(const RenderPassBeginInfo& info) = 0;
    virtual void endRenderPass() = 0;

    virtual void setPipeline(PipelineHandle pipeline) = 0;
    virtual void setVertexBuffer(uint32_t slot, BufferHandle buffer) = 0;
    virtual void setUniformBuffer(uint32_t binding, BufferHandle buffer) = 0;
    virtual void draw(uint32_t vertex_count, uint32_t first_vertex = 0) = 0;
};
} // namespace lunalite::rhi
