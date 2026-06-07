#pragma once
#include "../../asset/asset.h"
#include "renderer_common.h"
#include "TinyRHI/interface/device.h"

#include <cstddef>

#include <unordered_map>

namespace lunalite::renderer {

class TextureGpuCache {
public:
    TextureGpuCache(rhi::Device& device, rhi::CommandListHandle upload_command_list);
    ~TextureGpuCache();

    TextureGpuCache(const TextureGpuCache&) = delete;
    TextureGpuCache& operator=(const TextureGpuCache&) = delete;

    const TextureGpuResource& getOrCreate(asset::AssetHandle handle, FallbackTexture fallback);
    const TextureGpuResource&
        getOrCreate(asset::AssetHandle handle, FallbackTexture fallback, interface::TextureColorSpace color_space);
    const TextureGpuResource* find(asset::AssetHandle handle) const;
    const TextureGpuResource* find(asset::AssetHandle handle, interface::TextureColorSpace color_space) const;

private:
    struct TextureCacheKey {
        asset::AssetHandle handle{0};
        interface::TextureColorSpace color_space{interface::TextureColorSpace::SRGB};

        bool operator==(const TextureCacheKey& other) const
        {
            return handle == other.handle && color_space == other.color_space;
        }
    };

    struct TextureCacheKeyHash {
        size_t operator()(const TextureCacheKey& key) const noexcept;
    };

    const TextureGpuResource& getOrCreateFallback(FallbackTexture fallback);
    const TextureGpuResource& getOrCreateInternal(asset::AssetHandle handle,
                                                  FallbackTexture fallback,
                                                  interface::TextureColorSpace color_space);

    rhi::Device* m_device{nullptr};
    rhi::CommandListHandle m_upload_command_list{};
    TextureGpuResource m_white_fallback_texture{};
    TextureGpuResource m_flat_normal_fallback_texture{};
    std::unordered_map<TextureCacheKey, TextureGpuResource, TextureCacheKeyHash> m_texture_gpu_cache;
};

} // namespace lunalite::renderer
