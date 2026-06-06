#include "../../core/log.h"
#include "material_gpu_cache.h"

namespace lunalite::renderer {

MaterialGpuCache::MaterialGpuCache(rhi::Device& device,
                                   rhi::BindGroupLayoutHandle material_texture_bind_group_layout,
                                   TextureGpuCache& texture_cache)
    : m_device(&device),
      m_material_texture_bind_group_layout(material_texture_bind_group_layout),
      m_texture_cache(&texture_cache)
{}

MaterialGpuCache::~MaterialGpuCache()
{
    for (const auto& [_, resource] : m_material_gpu_cache) {
        if (resource.bind_group) {
            m_device->destroyBindGroup(resource.bind_group);
        }
    }
    m_material_gpu_cache.clear();
}

const MaterialGpuResource& MaterialGpuCache::getOrCreate(const interface::MaterialParameters& parameters)
{
    const MaterialTextureKey key{
        .albedo = parameters.albedo_texture,
        .normal = parameters.normal_texture,
        .metallic_roughness = parameters.metallic_roughness_texture,
        .occlusion = parameters.occlusion_texture,
        .emission = parameters.emission_texture,
    };

    if (const auto it = m_material_gpu_cache.find(key); it != m_material_gpu_cache.end()) {
        return it->second;
    }

    const auto& albedo = m_texture_cache->getOrCreate(key.albedo, FallbackTexture::White);
    const auto& normal = m_texture_cache->getOrCreate(key.normal, FallbackTexture::FlatNormal);
    const auto& metallicRoughness = m_texture_cache->getOrCreate(key.metallic_roughness, FallbackTexture::White);
    const auto& occlusion = m_texture_cache->getOrCreate(key.occlusion, FallbackTexture::White);
    const auto& emission = m_texture_cache->getOrCreate(key.emission, FallbackTexture::White);

    const auto bindGroup = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_material_texture_bind_group_layout,
        .entries =
            {
                combinedTextureEntry(0, albedo.view, albedo.sampler),
                combinedTextureEntry(1, normal.view, normal.sampler),
                combinedTextureEntry(2, metallicRoughness.view, metallicRoughness.sampler),
                combinedTextureEntry(3, occlusion.view, occlusion.sampler),
                combinedTextureEntry(4, emission.view, emission.sampler),
            },
    });

    const auto textureSlotCount =
        static_cast<unsigned>(key.albedo.isValid()) + static_cast<unsigned>(key.normal.isValid()) +
        static_cast<unsigned>(key.metallic_roughness.isValid()) + static_cast<unsigned>(key.occlusion.isValid()) +
        static_cast<unsigned>(key.emission.isValid());
    LUNA_CORE_DEBUG("Created material GPU texture bind group (texture slots: {})", textureSlotCount);
    return m_material_gpu_cache.emplace(key, MaterialGpuResource{.bind_group = bindGroup}).first->second;
}

} // namespace lunalite::renderer
