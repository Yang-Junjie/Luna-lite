#pragma once
#include "../interface/material.h"
#include "renderer_common.h"
#include "texture_gpu_cache.h"
#include "TinyRHI/interface/device.h"

#include <unordered_map>

namespace lunalite::renderer {

class MaterialGpuCache {
public:
    MaterialGpuCache(rhi::Device& device,
                     rhi::BindGroupLayoutHandle material_texture_bind_group_layout,
                     TextureGpuCache& texture_cache);
    ~MaterialGpuCache();

    MaterialGpuCache(const MaterialGpuCache&) = delete;
    MaterialGpuCache& operator=(const MaterialGpuCache&) = delete;

    const MaterialGpuResource& getOrCreate(const interface::MaterialParameters& parameters);

private:
    rhi::Device* m_device{nullptr};
    rhi::BindGroupLayoutHandle m_material_texture_bind_group_layout{};
    TextureGpuCache* m_texture_cache{nullptr};
    std::unordered_map<MaterialTextureKey, MaterialGpuResource, MaterialTextureKeyHash> m_material_gpu_cache;
};

} // namespace lunalite::renderer
