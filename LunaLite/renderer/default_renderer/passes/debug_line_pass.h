#pragma once
#include "../../interface/frame_render_data.h"
#include "../renderer_common.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/device.h"

#include <span>
#include <vector>

namespace lunalite::renderer {

class DebugLinePass {
public:
    DebugLinePass(rhi::Device& device,
                  rhi::CommandList& commands,
                  rhi::PipelineHandle line_depth_pipeline,
                  rhi::PipelineHandle line_overlay_pipeline,
                  rhi::BindGroupHandle geometry_bind_group,
                  rhi::BufferHandle frame_uniform_buffer,
                  rhi::BufferHandle object_uniform_buffer,
                  FrameUniforms& frame_uniforms,
                  bool& frame_uniforms_dirty);
    ~DebugLinePass();

    DebugLinePass(const DebugLinePass&) = delete;
    DebugLinePass& operator=(const DebugLinePass&) = delete;

    uint32_t execute(const GBuffer& gbuffer, std::span<const interface::LineDrawCommand> line_commands);

private:
    void flushFrameUniforms();
    void ensureVertexBuffer(size_t vertex_count);
    uint32_t renderBatch(const GBuffer& gbuffer,
                         rhi::PipelineHandle pipeline,
                         std::span<const LineVertex> vertices,
                         bool depth_test);

    rhi::Device* m_device{nullptr};
    rhi::CommandList* m_cmd{nullptr};
    rhi::PipelineHandle m_line_depth_pipeline{};
    rhi::PipelineHandle m_line_overlay_pipeline{};
    rhi::BindGroupHandle m_geometry_bind_group{};
    rhi::BufferHandle m_frame_uniform_buffer{};
    rhi::BufferHandle m_object_uniform_buffer{};
    rhi::BufferHandle m_line_vertex_buffer{};
    size_t m_line_vertex_buffer_capacity{0};
    std::vector<LineVertex> m_depth_test_vertices;
    std::vector<LineVertex> m_overlay_vertices;
    FrameUniforms* m_frame_uniforms{nullptr};
    bool* m_frame_uniforms_dirty{nullptr};
    ObjectUniforms m_object_uniforms;
};

} // namespace lunalite::renderer
