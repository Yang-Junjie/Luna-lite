#pragma once
#include "../../interface/frame_render_data.h"
#include "../renderer_common.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/device.h"

namespace lunalite::renderer {

class DebugLinePass {
public:
    DebugLinePass(rhi::Device& device,
                  rhi::CommandList& commands,
                  rhi::PipelineHandle line_pipeline,
                  rhi::PipelineHandle geometry_pipeline,
                  rhi::BindGroupHandle geometry_bind_group,
                  rhi::BufferHandle frame_uniform_buffer,
                  rhi::BufferHandle object_uniform_buffer,
                  FrameUniforms& frame_uniforms,
                  bool& frame_uniforms_dirty);
    ~DebugLinePass();

    DebugLinePass(const DebugLinePass&) = delete;
    DebugLinePass& operator=(const DebugLinePass&) = delete;

    void renderLine(const interface::LineDrawCommand& line_command);

private:
    void flushFrameUniforms();

    rhi::Device* m_device{nullptr};
    rhi::CommandList* m_cmd{nullptr};
    rhi::PipelineHandle m_line_pipeline{};
    rhi::PipelineHandle m_geometry_pipeline{};
    rhi::BindGroupHandle m_geometry_bind_group{};
    rhi::BufferHandle m_frame_uniform_buffer{};
    rhi::BufferHandle m_object_uniform_buffer{};
    rhi::BufferHandle m_line_vertex_buffer{};
    FrameUniforms* m_frame_uniforms{nullptr};
    bool* m_frame_uniforms_dirty{nullptr};
    ObjectUniforms m_object_uniforms;
};

} // namespace lunalite::renderer
