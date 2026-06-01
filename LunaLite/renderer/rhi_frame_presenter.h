#pragma once

#include "interface/frame_image.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"

#include <cstdint>

namespace lunalite::renderer {

class RHIFramePresenter {
public:
    RHIFramePresenter(rhi::Device& device, rhi::Swapchain& swapchain);
    ~RHIFramePresenter();

    void renderToSwapchain(const interface::FrameImage& image);
    void present();
    void present(const interface::FrameImage& image);

private:
    void ensureUploadTexture(const interface::FrameImage& image);
    void uploadCpuImage(const interface::FrameImage& image, const interface::CpuFrameStorage& storage);
    void drawToSwapchain(rhi::TextureViewHandle view);

    rhi::Device& m_device;
    rhi::Swapchain& m_swapchain;
    rhi::CommandList* m_cmd{nullptr};

    rhi::BindGroupLayoutHandle m_bind_group_layout{};
    rhi::PipelineLayoutHandle m_pipeline_layout{};
    rhi::PipelineHandle m_pipeline{};
    rhi::ShaderHandle m_vertex_shader{};
    rhi::ShaderHandle m_fragment_shader{};
    rhi::SamplerHandle m_sampler{};
    rhi::BindGroupHandle m_bind_group{};

    rhi::TextureHandle m_upload_texture{};
    rhi::TextureViewHandle m_upload_view{};
    uint32_t m_upload_width{0};
    uint32_t m_upload_height{0};
    interface::FrameImageFormat m_upload_format{interface::FrameImageFormat::RGBA8_UNorm};
};

} // namespace lunalite::renderer
