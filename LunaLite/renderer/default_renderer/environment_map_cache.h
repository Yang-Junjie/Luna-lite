#pragma once
#include "../../asset/asset.h"
#include "renderer_common.h"
#include "texture_gpu_cache.h"
#include "TinyRHI/interface/device.h"

#include <unordered_map>

namespace lunalite::renderer {

class EnvironmentMapCache {
public:
    EnvironmentMapCache(rhi::Device& device,
                        rhi::CommandListHandle upload_command_list,
                        rhi::CommandListHandle compute_command_list,
                        rhi::CommandList& compute_command_list_ref,
                        rhi::BindGroupLayoutHandle environment_bind_group_layout,
                        rhi::BindGroupLayoutHandle environment_compute_bind_group_layout,
                        rhi::PipelineHandle environment_cubemap_pipeline,
                        rhi::PipelineHandle environment_irradiance_pipeline,
                        rhi::PipelineHandle environment_prefilter_pipeline,
                        TextureGpuCache& texture_cache);
    ~EnvironmentMapCache();

    EnvironmentMapCache(const EnvironmentMapCache&) = delete;
    EnvironmentMapCache& operator=(const EnvironmentMapCache&) = delete;

    const EnvironmentGpuResource* getOrCreate(asset::AssetHandle handle);

    const EnvironmentGpuResource& fallback() const
    {
        return m_black_environment_gpu_resource;
    }

    rhi::BindGroupHandle bindGroup() const
    {
        return m_environment_bind_group;
    }

    void updateBindGroup(const EnvironmentGpuResource& resource);

private:
    void createBlackEnvironmentGpuResource();
    void createBrdfLutResource();

    rhi::Device* m_device{nullptr};
    rhi::CommandListHandle m_upload_command_list{};
    rhi::CommandListHandle m_compute_command_list{};
    rhi::CommandList* m_compute_cmd{nullptr};
    rhi::BindGroupLayoutHandle m_environment_bind_group_layout{};
    rhi::BindGroupLayoutHandle m_environment_compute_bind_group_layout{};
    rhi::PipelineHandle m_environment_cubemap_pipeline{};
    rhi::PipelineHandle m_environment_irradiance_pipeline{};
    rhi::PipelineHandle m_environment_prefilter_pipeline{};
    TextureGpuCache* m_texture_cache{nullptr};

    rhi::BindGroupHandle m_environment_bind_group{};
    TextureGpuResource m_brdf_lut_resource{};
    EnvironmentGpuResource m_black_environment_gpu_resource{};
    std::unordered_map<asset::AssetHandle, EnvironmentGpuResource> m_environment_gpu_cache;
};

} // namespace lunalite::renderer
