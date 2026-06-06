#pragma once
#include "../../asset/asset.h"
#include "renderer_common.h"
#include "TinyRHI/interface/device.h"

#include <unordered_map>

namespace lunalite::renderer {

class TextureGpuCache {
public:
    TextureGpuCache(rhi::Device& device, rhi::CommandListHandle upload_command_list);
    ~TextureGpuCache();

    TextureGpuCache(const TextureGpuCache&) = delete;
    TextureGpuCache& operator=(const TextureGpuCache&) = delete;

    const TextureGpuResource& getOrCreate(asset::AssetHandle handle, FallbackTexture fallback);
    const TextureGpuResource* find(asset::AssetHandle handle) const;

private:
    const TextureGpuResource& getOrCreateFallback(FallbackTexture fallback);

    rhi::Device* m_device{nullptr};
    rhi::CommandListHandle m_upload_command_list{};
    TextureGpuResource m_white_fallback_texture{};
    TextureGpuResource m_flat_normal_fallback_texture{};
    std::unordered_map<asset::AssetHandle, TextureGpuResource> m_texture_gpu_cache;
};

} // namespace lunalite::renderer
