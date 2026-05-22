
#pragma once
#include "command_context.h"
#include "rhi_types.h"

namespace lunalite::rhi {
class Device {
public:
    virtual ~Device() = default;
    virtual BufferHandle createBuffer(const BufferDesc& desc, const void* data) = 0;
    virtual ShaderHandle createShader(const ShaderDesc& desc) = 0;
    virtual PipelineHandle createPipeline(const PipelineDesc& desc) = 0;
    virtual CommandContext& getImmediateCmdContext() = 0;
};
} // namespace lunalite::rhi
