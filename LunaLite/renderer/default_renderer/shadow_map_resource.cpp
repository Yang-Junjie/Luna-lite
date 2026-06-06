#include "../../core/log.h"
#include "shadow_map_resource.h"

#include <algorithm>

namespace lunalite::renderer {

ShadowMapResource::ShadowMapResource(rhi::Device& device, rhi::BindGroupLayoutHandle shadow_lighting_bind_group_layout)
    : m_device(&device),
      m_shadow_lighting_bind_group_layout(shadow_lighting_bind_group_layout)
{
    m_lighting_uniform_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(ShadowLightingUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::UniformRead,
    });
    LUNA_ASSERT(m_lighting_uniform_buffer, "Failed to create shadow lighting uniform buffer.");

    createTexture(1, 1, rhi::ResourceState::ShaderRead);

    ShadowLightingUniforms disabledUniforms;
    disabledUniforms.texelSizeBiasNormalBias = glm::vec4{1.0f, 1.0f, 0.0f, 0.0f};
    updateLightingUniforms(disabledUniforms);
}

ShadowMapResource::~ShadowMapResource()
{
    destroy();
}

void ShadowMapResource::ensure(const interface::RenderShadowSettings& settings)
{
    const auto requestedSize = std::max(settings.map_size, 1u);
    const auto requestedCascadeCount = std::clamp(settings.cascade_count, 1u, MaxShadowCascades);
    if (m_texture && m_view && m_sampler && m_lighting_bind_group && m_size == requestedSize &&
        m_cascade_count == requestedCascadeCount) {
        return;
    }

    createTexture(requestedSize, requestedCascadeCount, rhi::ResourceState::DepthStencilWrite);
}

void ShadowMapResource::updateLightingUniforms(const ShadowLightingUniforms& uniforms)
{
    LUNA_ASSERT(m_lighting_uniform_buffer, "Shadow lighting uniform buffer is not initialized.");
    m_device->updateBuffer(m_lighting_uniform_buffer, 0, &uniforms, sizeof(ShadowLightingUniforms));
}

void ShadowMapResource::createTexture(uint32_t size, uint32_t cascade_count, rhi::ResourceState initial_state)
{
    destroyLightingBindGroup();
    destroyTexture();

    m_size = std::max(size, 1u);
    m_cascade_count = std::clamp(cascade_count, 1u, MaxShadowCascades);
    m_layer_count = std::max(m_cascade_count, 2u);
    m_texture = m_device->createTexture(rhi::TextureDesc{
        .width = m_size,
        .height = m_size,
        .dimension = rhi::TextureDimension::Texture2D,
        .format = rhi::TextureFormat::Depth32F,
        .usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled,
        .array_layers = m_layer_count,
        .initial_state = initial_state,
    });
    m_view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_texture,
        .view_dimension = rhi::TextureViewDimension::Texture2DArray,
        .format = rhi::TextureFormat::Depth32F,
        .aspect = rhi::TextureAspect::Depth,
        .array_layer_count = m_layer_count,
    });
    for (uint32_t cascadeIndex = 0; cascadeIndex < m_cascade_count; ++cascadeIndex) {
        m_layer_views[cascadeIndex] = m_device->createTextureView(rhi::TextureViewDesc{
            .texture = m_texture,
            .view_dimension = rhi::TextureViewDimension::Texture2D,
            .format = rhi::TextureFormat::Depth32F,
            .aspect = rhi::TextureAspect::Depth,
            .base_array_layer = cascadeIndex,
            .array_layer_count = 1,
        });
    }
    if (!m_sampler) {
        m_sampler = m_device->createSampler(rhi::SamplerDesc{
            .min_filter = rhi::FilterMode::Linear,
            .mag_filter = rhi::FilterMode::Linear,
            .mip_filter = rhi::MipFilter::None,
            .address_u = rhi::AddressMode::ClampToEdge,
            .address_v = rhi::AddressMode::ClampToEdge,
            .address_w = rhi::AddressMode::ClampToEdge,
        });
    }

    LUNA_ASSERT(m_texture, "Failed to create shadow map texture.");
    LUNA_ASSERT(m_view, "Failed to create shadow map array view.");
    for (uint32_t cascadeIndex = 0; cascadeIndex < m_cascade_count; ++cascadeIndex) {
        LUNA_ASSERT(m_layer_views[cascadeIndex], "Failed to create shadow map layer view.");
    }
    LUNA_ASSERT(m_sampler, "Failed to create shadow map sampler.");
    createLightingBindGroup();
    LUNA_CORE_DEBUG("Created shadow map resource ({}x{}x{})", m_size, m_size, m_cascade_count);
}

void ShadowMapResource::createLightingBindGroup()
{
    LUNA_ASSERT(m_shadow_lighting_bind_group_layout, "Shadow lighting bind group layout is not initialized.");
    LUNA_ASSERT(m_lighting_uniform_buffer, "Shadow lighting uniform buffer is not initialized.");
    LUNA_ASSERT(m_view, "Shadow map view is not initialized.");
    LUNA_ASSERT(m_sampler, "Shadow map sampler is not initialized.");

    m_lighting_bind_group = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_shadow_lighting_bind_group_layout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_lighting_uniform_buffer,
                            .offset = 0,
                            .size = sizeof(ShadowLightingUniforms),
                        },
                },
                combinedTextureEntry(1, m_view, m_sampler),
            },
    });
    LUNA_ASSERT(m_lighting_bind_group, "Failed to create shadow lighting bind group.");
}

void ShadowMapResource::destroyLightingBindGroup()
{
    if (m_lighting_bind_group) {
        m_device->destroyBindGroup(m_lighting_bind_group);
        m_lighting_bind_group = {};
    }
}

void ShadowMapResource::destroyTexture()
{
    for (auto& layerView : m_layer_views) {
        if (layerView) {
            m_device->destroyTextureView(layerView);
            layerView = {};
        }
    }
    if (m_view) {
        m_device->destroyTextureView(m_view);
        m_view = {};
    }
    if (m_texture) {
        m_device->destroyTexture(m_texture);
        m_texture = {};
    }
    m_size = 0;
    m_cascade_count = 0;
    m_layer_count = 0;
}

void ShadowMapResource::destroy()
{
    destroyLightingBindGroup();
    destroyTexture();

    if (m_sampler) {
        m_device->destroySampler(m_sampler);
        m_sampler = {};
    }
    if (m_lighting_uniform_buffer) {
        m_device->destroyBuffer(m_lighting_uniform_buffer);
        m_lighting_uniform_buffer = {};
    }
}

} // namespace lunalite::renderer
