#include "../../core/log.h"
#include "gbuffer_resource.h"

namespace lunalite::renderer {

GBufferResource::GBufferResource(rhi::Device& device,
                                 rhi::BindGroupLayoutHandle lighting_bind_group_layout,
                                 rhi::BufferHandle frame_uniform_buffer)
    : m_device(&device),
      m_lighting_bind_group_layout(lighting_bind_group_layout),
      m_frame_uniform_buffer(frame_uniform_buffer)
{
    m_sampler = m_device->createSampler(rhi::SamplerDesc{
        .min_filter = rhi::FilterMode::Nearest,
        .mag_filter = rhi::FilterMode::Nearest,
        .address_u = rhi::AddressMode::ClampToEdge,
        .address_v = rhi::AddressMode::ClampToEdge,
    });
    LUNA_ASSERT(m_sampler, "Failed to create GBuffer sampler.");
}

GBufferResource::~GBufferResource()
{
    destroy();
    if (m_sampler) {
        m_device->destroySampler(m_sampler);
        m_sampler = {};
    }
}

void GBufferResource::ensure(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    if (m_gbuffer.width == width && m_gbuffer.height == height && m_gbuffer.lighting_bind_group) {
        return;
    }

    destroy();
    m_gbuffer = GBuffer{.width = width, .height = height};

    m_gbuffer.albedo_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.normal_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA16F,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.material_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.emission_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA16F,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });
    m_gbuffer.depth_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::Depth24Stencil8,
        .usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::DepthStencilWrite,
    });
    m_gbuffer.final_color_texture = m_device->createTexture(rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled,
        .initial_state = rhi::ResourceState::ColorAttachment,
    });

    m_gbuffer.albedo_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.albedo_texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.normal_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.normal_texture,
        .format = rhi::TextureFormat::RGBA16F,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.material_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.material_texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.emission_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.emission_texture,
        .format = rhi::TextureFormat::RGBA16F,
        .aspect = rhi::TextureAspect::Color,
    });
    m_gbuffer.depth_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.depth_texture,
        .format = rhi::TextureFormat::Depth24Stencil8,
        .aspect = rhi::TextureAspect::DepthStencil,
    });
    m_gbuffer.final_color_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_gbuffer.final_color_texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });

    m_gbuffer.lighting_bind_group = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_lighting_bind_group_layout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_frame_uniform_buffer,
                            .offset = 0,
                            .size = sizeof(FrameUniforms),
                        },
                },
                combinedTextureEntry(1, m_gbuffer.albedo_view, m_sampler),
                combinedTextureEntry(2, m_gbuffer.normal_view, m_sampler),
                combinedTextureEntry(3, m_gbuffer.material_view, m_sampler),
                combinedTextureEntry(4, m_gbuffer.depth_view, m_sampler),
                combinedTextureEntry(5, m_gbuffer.emission_view, m_sampler),
            },
    });

    LUNA_ASSERT(m_gbuffer.albedo_texture, "Failed to create GBuffer albedo texture.");
    LUNA_ASSERT(m_gbuffer.normal_texture, "Failed to create GBuffer normal texture.");
    LUNA_ASSERT(m_gbuffer.material_texture, "Failed to create GBuffer material texture.");
    LUNA_ASSERT(m_gbuffer.emission_texture, "Failed to create GBuffer emission texture.");
    LUNA_ASSERT(m_gbuffer.depth_texture, "Failed to create GBuffer depth texture.");
    LUNA_ASSERT(m_gbuffer.final_color_texture, "Failed to create GBuffer final color texture.");
    LUNA_ASSERT(m_gbuffer.albedo_view, "Failed to create GBuffer albedo view.");
    LUNA_ASSERT(m_gbuffer.normal_view, "Failed to create GBuffer normal view.");
    LUNA_ASSERT(m_gbuffer.material_view, "Failed to create GBuffer material view.");
    LUNA_ASSERT(m_gbuffer.emission_view, "Failed to create GBuffer emission view.");
    LUNA_ASSERT(m_gbuffer.depth_view, "Failed to create GBuffer depth view.");
    LUNA_ASSERT(m_gbuffer.final_color_view, "Failed to create GBuffer final color view.");
    LUNA_ASSERT(m_gbuffer.lighting_bind_group, "Failed to create GBuffer lighting bind group.");
    LUNA_CORE_DEBUG("GBuffer resized to {}x{}", width, height);

    m_frame_image = interface::FrameImage{
        .width = width,
        .height = height,
        .format = interface::FrameImageFormat::RGBA8_UNorm,
        .color_space = interface::FrameImageColorSpace::SRGB,
        .storage =
            interface::GpuFrameStorage{
                .texture = m_gbuffer.final_color_texture,
                .view = m_gbuffer.final_color_view,
            },
    };
}

void GBufferResource::destroy()
{
    if (m_gbuffer.lighting_bind_group) {
        m_device->destroyBindGroup(m_gbuffer.lighting_bind_group);
    }

    const rhi::TextureViewHandle views[] = {
        m_gbuffer.albedo_view,
        m_gbuffer.normal_view,
        m_gbuffer.material_view,
        m_gbuffer.emission_view,
        m_gbuffer.depth_view,
        m_gbuffer.final_color_view,
    };
    for (const auto view : views) {
        if (view) {
            m_device->destroyTextureView(view);
        }
    }

    const rhi::TextureHandle textures[] = {
        m_gbuffer.albedo_texture,
        m_gbuffer.normal_texture,
        m_gbuffer.material_texture,
        m_gbuffer.emission_texture,
        m_gbuffer.depth_texture,
        m_gbuffer.final_color_texture,
    };
    for (const auto texture : textures) {
        if (texture) {
            m_device->destroyTexture(texture);
        }
    }

    m_gbuffer = {};
    m_frame_image = {};
}

} // namespace lunalite::renderer
