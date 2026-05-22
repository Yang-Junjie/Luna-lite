#pragma once
#include "../../rhi/interface/instance.h"
#include "../interface/renderer.h"

#include <cstdint>

#include <vector>

namespace lunalite::renderer {
class Renderer : public interface::Renderer {
public:
    explicit Renderer(rhi::Instance& rhi);
    ~Renderer() override = default;
    void beginFrame() override;
    void endFrame() override;
    void renderTriangle(const interface::Triangle& triangle) override;

private:
    rhi::Device* m_device = nullptr;
    rhi::CommandContext* m_cmd = nullptr;
    rhi::PipelineHandle m_pipeline = 0;
    std::vector<rhi::BufferHandle> m_triangle_buffers;
    size_t m_frame_triangle_index = 0;
};
} // namespace lunalite::renderer
