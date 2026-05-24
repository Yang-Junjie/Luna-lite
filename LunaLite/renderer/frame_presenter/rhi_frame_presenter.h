#pragma once

#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"
#include "../interface/frame_image.h"

#include <cstdint>

namespace lunalite::renderer {

class RHIFramePresenter {
public:
    RHIFramePresenter(rhi::Device& device, rhi::Swapchain& swapchain);
    ~RHIFramePresenter();

    void present(const interface::FrameImage& image);

private:
    void ensureUploadTexture(const interface::FrameImage& image);
    void uploadCpuImage(const interface::FrameImage& image, const interface::CpuFrameStorage& storage);
    void drawToSwapchain(rhi::TextureViewHandle view, interface::FrameImageColorSpace color_space);

    rhi::Device& m_device;
    rhi::Swapchain& m_swapchain;
    rhi::CommandList* m_cmd{nullptr};

    rhi::BindGroupLayoutHandle m_bind_group_layout{0};
    rhi::PipelineLayoutHandle m_pipeline_layout{0};
    rhi::PipelineHandle m_pipeline{0};
    rhi::ShaderHandle m_vertex_shader{0};
    rhi::ShaderHandle m_fragment_shader{0};
    rhi::SamplerHandle m_sampler{0};
    rhi::BufferHandle m_uniform_buffer{0};
    rhi::BindGroupHandle m_bind_group{0};

    rhi::TextureHandle m_upload_texture{0};
    rhi::TextureViewHandle m_upload_view{0};
    uint32_t m_upload_width{0};
    uint32_t m_upload_height{0};
    interface::FrameImageFormat m_upload_format{interface::FrameImageFormat::RGBA8_UNorm};
};

} // namespace lunalite::renderer
