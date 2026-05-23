#pragma once
#include "../interface/command_list.h"

#include <glad/glad.h>

namespace lunalite::rhi {

class OpenGLDevice;

class OpenGLCommandList final : public CommandList {
public:
    explicit OpenGLCommandList(OpenGLDevice& device);
    ~OpenGLCommandList() override = default;

    void begin() override;
    void end() override;
    void beginRenderPass(const RenderPassBeginInfo& info) override;
    void endRenderPass() override;
    void setPipeline(PipelineHandle pipeline) override;
    void setVertexBuffer(uint32_t slot, BufferHandle buffer) override;
    void setUniformBuffer(uint32_t binding, BufferHandle buffer) override;
    void draw(uint32_t vertex_count, uint32_t first_vertex = 0) override;

private:
    OpenGLDevice& m_device;
    PipelineHandle m_current_pipeline{0};
    GLuint m_render_pass_fbo{0};
};

} // namespace lunalite::rhi
