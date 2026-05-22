#pragma once
#include "../interface/command_context.h"

namespace lunalite::rhi {

class OpenGLDevice;

class OpenGLCommandContext final : public CommandContext {
public:
    OpenGLCommandContext(OpenGLDevice& device, void* native_window);
    ~OpenGLCommandContext() override = default;

    void beginFrame() override;
    void clear(float r, float g, float b, float a) override;
    void bindPipeline(PipelineHandle pipeline) override;
    void bindVertexBuffer(BufferHandle buffer) override;
    void draw(uint32_t vertex_count, uint32_t first_vertex = 0) override;
    void endFrame() override;
    void present() override;

private:
    OpenGLDevice& m_device;
    void* m_native_window = nullptr;
    PipelineHandle m_current_pipeline = 0;
};

} // namespace lunalite::rhi
