#include "../../asset/asset_database.h"
#include "../../core/log.h"
#include "../rhi_upload_helpers.h"
#include "texture_gpu_cache.h"

#include <array>

namespace lunalite::renderer {

TextureGpuCache::TextureGpuCache(rhi::Device& device, rhi::CommandListHandle upload_command_list)
    : m_device(&device),
      m_upload_command_list(upload_command_list)
{}

TextureGpuCache::~TextureGpuCache()
{
    for (auto& [_, resource] : m_texture_gpu_cache) {
        destroyTextureGpuResource(*m_device, resource);
    }
    m_texture_gpu_cache.clear();

    destroyTextureGpuResource(*m_device, m_white_fallback_texture);
    destroyTextureGpuResource(*m_device, m_flat_normal_fallback_texture);
}

const TextureGpuResource& TextureGpuCache::getOrCreate(asset::AssetHandle handle, FallbackTexture fallback)
{
    if (!handle.isValid()) {
        return getOrCreateFallback(fallback);
    }

    if (const auto it = m_texture_gpu_cache.find(handle); it != m_texture_gpu_cache.end()) {
        return it->second;
    }

    const auto* texture = asset::AssetDatabase::get().get<interface::Texture>(handle);
    if (texture == nullptr || texture->getWidth() == 0 || texture->getHeight() == 0 || texture->getPixels().empty()) {
        return getOrCreateFallback(fallback);
    }

    const auto& settings = texture->getImportSettings();
    const auto rhiFormat = toRHITextureFormat(texture->getFormat(), settings);
    const auto mipLevels = settings.generate_mipmaps ? fullMipLevelCount(texture->getWidth(), texture->getHeight()) : 1;

    TextureGpuResource resource;
    resource.texture = m_device->createTexture(rhi::TextureDesc{
        .width = texture->getWidth(),
        .height = texture->getHeight(),
        .format = rhiFormat,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst |
                 (mipLevels > 1 ? rhi::TextureUsage::CopySrc : rhi::TextureUsage::None),
        .mip_levels = mipLevels,
        .initial_state = rhi::ResourceState::CopyDst,
    });
    if (!resource.texture) {
        return getOrCreateFallback(fallback);
    }

    if (!rhi_upload::uploadTextureData(
            *m_device,
            m_upload_command_list,
            resource.texture,
            rhi_upload::TextureUploadData{
                .data = texture->getPixels().data(),
                .size = static_cast<size_t>(texture->getHeight()) * texture->getWidth() *
                        textureBytesPerPixel(texture->getFormat()),
                .row_pitch = static_cast<size_t>(texture->getWidth()) * textureBytesPerPixel(texture->getFormat()),
                .width = texture->getWidth(),
                .height = texture->getHeight(),
            },
            mipLevels > 1)) {
        destroyTextureGpuResource(*m_device, resource);
        return getOrCreateFallback(fallback);
    }

    resource.view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = resource.texture,
        .format = rhiFormat,
        .aspect = rhi::TextureAspect::Color,
        .mip_level_count = mipLevels,
    });

    auto samplerDesc = toRHISamplerDesc(settings.sampler);
    if (mipLevels == 1) {
        samplerDesc.mip_filter = rhi::MipFilter::None;
    }
    resource.sampler = m_device->createSampler(samplerDesc);

    if (!resource.view || !resource.sampler) {
        destroyTextureGpuResource(*m_device, resource);
        return getOrCreateFallback(fallback);
    }

    LUNA_CORE_DEBUG("Created GPU texture resource for asset {} ({}x{}, mip levels: {})",
                    handle.toString(),
                    texture->getWidth(),
                    texture->getHeight(),
                    mipLevels);
    return m_texture_gpu_cache.emplace(handle, resource).first->second;
}

const TextureGpuResource* TextureGpuCache::find(asset::AssetHandle handle) const
{
    const auto it = m_texture_gpu_cache.find(handle);
    return it != m_texture_gpu_cache.end() ? &it->second : nullptr;
}

const TextureGpuResource& TextureGpuCache::getOrCreateFallback(FallbackTexture fallback)
{
    auto& resource =
        fallback == FallbackTexture::FlatNormal ? m_flat_normal_fallback_texture : m_white_fallback_texture;
    if (resource.texture && resource.view && resource.sampler) {
        return resource;
    }

    const std::array<uint8_t, 4> pixel = fallback == FallbackTexture::FlatNormal
                                             ? std::array<uint8_t, 4>{128, 128, 255, 255}
                                             : std::array<uint8_t, 4>{255, 255, 255, 255};

    resource.texture = m_device->createTexture(rhi::TextureDesc{
        .width = 1,
        .height = 1,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst,
        .initial_state = rhi::ResourceState::CopyDst,
    });
    resource.view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = resource.texture,
        .format = rhi::TextureFormat::RGBA8_UNorm,
        .aspect = rhi::TextureAspect::Color,
    });
    resource.sampler = m_device->createSampler(fallbackSamplerDesc());

    if (resource.texture) {
        rhi_upload::uploadTextureData(*m_device,
                                      m_upload_command_list,
                                      resource.texture,
                                      rhi_upload::TextureUploadData{
                                          .data = pixel.data(),
                                          .size = pixel.size(),
                                          .row_pitch = pixel.size(),
                                          .width = 1,
                                          .height = 1,
                                      });
    }

    LUNA_CORE_DEBUG("Created {} fallback GPU texture",
                    fallback == FallbackTexture::FlatNormal ? "flat normal" : "white");
    return resource;
}

} // namespace lunalite::renderer
