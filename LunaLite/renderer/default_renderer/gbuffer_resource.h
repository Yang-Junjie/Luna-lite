#pragma once
#include "../interface/frame_image.h"
#include "renderer_common.h"
#include "TinyRHI/interface/device.h"

#include <cstdint>

namespace lunalite::renderer {

class GBufferResource {
public:
    GBufferResource(rhi::Device& device,
                    rhi::BindGroupLayoutHandle lighting_bind_group_layout,
                    rhi::BufferHandle frame_uniform_buffer);
    ~GBufferResource();

    GBufferResource(const GBufferResource&) = delete;
    GBufferResource& operator=(const GBufferResource&) = delete;

    void ensure(uint32_t width, uint32_t height);

    const GBuffer& get() const
    {
        return m_gbuffer;
    }

    const interface::FrameImage& getFrameImage() const
    {
        return m_frame_image;
    }

    rhi::SamplerHandle sampler() const
    {
        return m_sampler;
    }

private:
    void destroy();

    rhi::Device* m_device{nullptr};
    rhi::BindGroupLayoutHandle m_lighting_bind_group_layout{};
    rhi::BufferHandle m_frame_uniform_buffer{};
    rhi::SamplerHandle m_sampler{};
    GBuffer m_gbuffer{};
    interface::FrameImage m_frame_image;
};

} // namespace lunalite::renderer
