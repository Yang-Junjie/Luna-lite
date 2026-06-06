#pragma once
#include "../interface/render_lighting.h"
#include "renderer_common.h"
#include "TinyRHI/interface/device.h"

#include <cstdint>

namespace lunalite::renderer {

class ShadowMapResource {
public:
    ShadowMapResource(rhi::Device& device, rhi::BindGroupLayoutHandle shadow_lighting_bind_group_layout);
    ~ShadowMapResource();

    ShadowMapResource(const ShadowMapResource&) = delete;
    ShadowMapResource& operator=(const ShadowMapResource&) = delete;

    void ensure(const interface::RenderShadowSettings& settings);
    void updateLightingUniforms(const ShadowLightingUniforms& uniforms);

    uint32_t size() const
    {
        return m_size;
    }

    rhi::TextureHandle texture() const
    {
        return m_texture;
    }

    rhi::TextureViewHandle view() const
    {
        return m_view;
    }

    rhi::SamplerHandle sampler() const
    {
        return m_sampler;
    }

    rhi::BindGroupHandle lightingBindGroup() const
    {
        return m_lighting_bind_group;
    }

private:
    void createTexture(uint32_t size, rhi::ResourceState initial_state);
    void createLightingBindGroup();
    void destroyLightingBindGroup();
    void destroyTexture();
    void destroy();

    rhi::Device* m_device{nullptr};
    rhi::BindGroupLayoutHandle m_shadow_lighting_bind_group_layout{};
    uint32_t m_size{0};
    rhi::TextureHandle m_texture{};
    rhi::TextureViewHandle m_view{};
    rhi::SamplerHandle m_sampler{};
    rhi::BufferHandle m_lighting_uniform_buffer{};
    rhi::BindGroupHandle m_lighting_bind_group{};
};

} // namespace lunalite::renderer
