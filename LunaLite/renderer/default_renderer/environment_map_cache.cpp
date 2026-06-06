#include "../../asset/asset_database.h"
#include "../../core/log.h"
#include "../rhi_upload_helpers.h"
#include "environment_map_cache.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace lunalite::renderer {

EnvironmentMapCache::EnvironmentMapCache(rhi::Device& device,
                                         rhi::CommandListHandle upload_command_list,
                                         rhi::CommandListHandle compute_command_list,
                                         rhi::CommandList& compute_command_list_ref,
                                         rhi::BindGroupLayoutHandle environment_bind_group_layout,
                                         rhi::BindGroupLayoutHandle environment_compute_bind_group_layout,
                                         rhi::PipelineHandle environment_cubemap_pipeline,
                                         rhi::PipelineHandle environment_irradiance_pipeline,
                                         rhi::PipelineHandle environment_prefilter_pipeline,
                                         TextureGpuCache& texture_cache)
    : m_device(&device),
      m_upload_command_list(upload_command_list),
      m_compute_command_list(compute_command_list),
      m_compute_cmd(&compute_command_list_ref),
      m_environment_bind_group_layout(environment_bind_group_layout),
      m_environment_compute_bind_group_layout(environment_compute_bind_group_layout),
      m_environment_cubemap_pipeline(environment_cubemap_pipeline),
      m_environment_irradiance_pipeline(environment_irradiance_pipeline),
      m_environment_prefilter_pipeline(environment_prefilter_pipeline),
      m_texture_cache(&texture_cache)
{
    createBlackEnvironmentGpuResource();
    createBrdfLutResource();
    m_environment_bind_group = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_environment_bind_group_layout,
        .entries =
            {
                combinedTextureEntry(0,
                                     m_black_environment_gpu_resource.environment.view,
                                     m_black_environment_gpu_resource.environment.sampler),
                combinedTextureEntry(1,
                                     m_black_environment_gpu_resource.irradiance.view,
                                     m_black_environment_gpu_resource.irradiance.sampler),
                combinedTextureEntry(2,
                                     m_black_environment_gpu_resource.prefilter.view,
                                     m_black_environment_gpu_resource.prefilter.sampler),
                combinedTextureEntry(3, m_brdf_lut_resource.view, m_brdf_lut_resource.sampler),
            },
    });

    LUNA_ASSERT(m_black_environment_gpu_resource.environment.texture, "Failed to create black environment texture.");
    LUNA_ASSERT(m_black_environment_gpu_resource.environment.view, "Failed to create black environment view.");
    LUNA_ASSERT(m_black_environment_gpu_resource.environment.sampler, "Failed to create black environment sampler.");
    LUNA_ASSERT(m_black_environment_gpu_resource.irradiance.texture, "Failed to create black irradiance texture.");
    LUNA_ASSERT(m_black_environment_gpu_resource.irradiance.view, "Failed to create black irradiance view.");
    LUNA_ASSERT(m_black_environment_gpu_resource.irradiance.sampler, "Failed to create black irradiance sampler.");
    LUNA_ASSERT(m_black_environment_gpu_resource.prefilter.texture, "Failed to create black prefilter texture.");
    LUNA_ASSERT(m_black_environment_gpu_resource.prefilter.view, "Failed to create black prefilter view.");
    LUNA_ASSERT(m_black_environment_gpu_resource.prefilter.sampler, "Failed to create black prefilter sampler.");
    LUNA_ASSERT(m_brdf_lut_resource.texture, "Failed to create BRDF LUT texture.");
    LUNA_ASSERT(m_brdf_lut_resource.view, "Failed to create BRDF LUT view.");
    LUNA_ASSERT(m_brdf_lut_resource.sampler, "Failed to create BRDF LUT sampler.");
    LUNA_ASSERT(m_environment_bind_group, "Failed to create environment bind group.");
}

EnvironmentMapCache::~EnvironmentMapCache()
{
    if (m_environment_bind_group) {
        m_device->destroyBindGroup(m_environment_bind_group);
        m_environment_bind_group = {};
    }

    for (auto& [_, resource] : m_environment_gpu_cache) {
        destroyEnvironmentGpuResource(*m_device, resource);
    }
    m_environment_gpu_cache.clear();

    destroyTextureGpuResource(*m_device, m_brdf_lut_resource);
    destroyEnvironmentGpuResource(*m_device, m_black_environment_gpu_resource);
}

const EnvironmentGpuResource* EnvironmentMapCache::getOrCreate(asset::AssetHandle handle)
{
    if (!handle.isValid()) {
        return nullptr;
    }

    const auto settings = readEnvironmentSettings(handle);
    if (!settings) {
        return nullptr;
    }

    const uint32_t environmentMipCount = fullMipLevelCount(DefaultEnvironmentCubeSize, DefaultEnvironmentCubeSize);
    const uint32_t prefilterMipCount = fullMipLevelCount(settings->prefilter_size, settings->prefilter_size);
    if (const auto it = m_environment_gpu_cache.find(handle); it != m_environment_gpu_cache.end()) {
        if (it->second.environment_size == DefaultEnvironmentCubeSize &&
            it->second.environment_mip_count == environmentMipCount &&
            it->second.irradiance_size == settings->irradiance_size &&
            it->second.prefilter_size == settings->prefilter_size &&
            it->second.prefilter_mip_count == prefilterMipCount) {
            return &it->second;
        }

        destroyEnvironmentGpuResource(*m_device, it->second);
        m_environment_gpu_cache.erase(it);
        LUNA_CORE_DEBUG("Rebuilding environment GPU resource for asset {} because import settings changed",
                        handle.toString());
    }

    const auto* texture = asset::AssetDatabase::get().get<interface::Texture>(handle);
    if (texture == nullptr || texture->getWidth() == 0 || texture->getHeight() == 0 || texture->getPixels().empty()) {
        return nullptr;
    }

    m_texture_cache->getOrCreate(handle, FallbackTexture::White);
    const auto* sourceResource = m_texture_cache->find(handle);
    if (sourceResource == nullptr || !sourceResource->view || !sourceResource->sampler) {
        return nullptr;
    }

    EnvironmentGpuResource resource;
    resource.environment_size = DefaultEnvironmentCubeSize;
    resource.environment_mip_count = environmentMipCount;
    resource.irradiance_size = settings->irradiance_size;
    resource.prefilter_size = settings->prefilter_size;
    resource.prefilter_mip_count = prefilterMipCount;

    resource.environment.texture = createCubeTexture(*m_device,
                                                     resource.environment_size,
                                                     resource.environment_mip_count,
                                                     EnvironmentCubeFormat,
                                                     rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage |
                                                         rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst,
                                                     rhi::ResourceState::StorageReadWrite);
    resource.environment.view = createCubeTextureView(
        *m_device, resource.environment.texture, EnvironmentCubeFormat, resource.environment_mip_count);
    resource.environment.sampler = createCubeSampler(*m_device, resource.environment_mip_count);
    if (!resource.environment.texture || !resource.environment.view || !resource.environment.sampler) {
        destroyEnvironmentGpuResource(*m_device, resource);
        return nullptr;
    }

    for (uint32_t face = 0; face < 6; ++face) {
        resource.environment_face_views[face] =
            createCubeFaceView(*m_device, resource.environment.texture, EnvironmentCubeFormat, 0, face);
        if (!resource.environment_face_views[face]) {
            destroyEnvironmentGpuResource(*m_device, resource);
            return nullptr;
        }
    }

    resource.irradiance.texture =
        createCubeTexture(*m_device,
                          resource.irradiance_size,
                          1,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst,
                          rhi::ResourceState::StorageReadWrite);
    resource.prefilter.texture =
        createCubeTexture(*m_device,
                          resource.prefilter_size,
                          resource.prefilter_mip_count,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst,
                          rhi::ResourceState::StorageReadWrite);
    if (!resource.irradiance.texture || !resource.prefilter.texture) {
        destroyEnvironmentGpuResource(*m_device, resource);
        return nullptr;
    }

    resource.irradiance.view =
        createCubeTextureView(*m_device, resource.irradiance.texture, FilteredEnvironmentCubeFormat, 1);
    resource.irradiance.sampler = createCubeSampler(*m_device, 1);
    resource.prefilter.view = createCubeTextureView(
        *m_device, resource.prefilter.texture, FilteredEnvironmentCubeFormat, resource.prefilter_mip_count);
    resource.prefilter.sampler = createCubeSampler(*m_device, resource.prefilter_mip_count);
    if (!resource.irradiance.view || !resource.irradiance.sampler || !resource.prefilter.view ||
        !resource.prefilter.sampler) {
        destroyEnvironmentGpuResource(*m_device, resource);
        return nullptr;
    }

    for (uint32_t face = 0; face < 6; ++face) {
        resource.irradiance_face_views[face] =
            createCubeFaceView(*m_device, resource.irradiance.texture, FilteredEnvironmentCubeFormat, 0, face);
        if (!resource.irradiance_face_views[face]) {
            destroyEnvironmentGpuResource(*m_device, resource);
            return nullptr;
        }
    }

    resource.prefilter_face_views.resize(static_cast<size_t>(resource.prefilter_mip_count) * 6);
    for (uint32_t mip = 0; mip < resource.prefilter_mip_count; ++mip) {
        for (uint32_t face = 0; face < 6; ++face) {
            const size_t index = static_cast<size_t>(mip) * 6 + face;
            resource.prefilter_face_views[index] =
                createCubeFaceView(*m_device, resource.prefilter.texture, FilteredEnvironmentCubeFormat, mip, face);
            if (!resource.prefilter_face_views[index]) {
                destroyEnvironmentGpuResource(*m_device, resource);
                return nullptr;
            }
        }
    }

    const auto makeComputeBindGroupDesc =
        [&](rhi::TextureViewHandle sourceView, rhi::SamplerHandle sourceSampler, rhi::TextureViewHandle outputView) {
            return rhi::BindGroupDesc{
                .layout = m_environment_compute_bind_group_layout,
                .entries =
                    {
                        combinedTextureEntry(0, sourceView, sourceSampler),
                        rhi::BindGroupEntry{
                            .binding = 1,
                            .type = rhi::BindingType::StorageTexture,
                            .texture_view = outputView,
                        },
                    },
            };
        };

    const auto computeBindGroup = m_device->createBindGroup(
        makeComputeBindGroupDesc(sourceResource->view, sourceResource->sampler, resource.environment_face_views[0]));
    if (!computeBindGroup) {
        destroyEnvironmentGpuResource(*m_device, resource);
        return nullptr;
    }

    const uint32_t environmentGroupCount =
        (resource.environment_size + EnvironmentComputeGroupSize - 1) / EnvironmentComputeGroupSize;
    m_compute_cmd->begin();

    const rhi::TextureTransition sourceRead{
        .texture = sourceResource->texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    const rhi::TextureTransition environmentStorage{
        .texture = resource.environment.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&sourceRead, 1);
    m_compute_cmd->transition(&environmentStorage, 1);
    m_compute_cmd->setPipeline(m_environment_cubemap_pipeline);

    for (uint32_t face = 0; face < 6; ++face) {
        m_device->updateBindGroup(computeBindGroup,
                                  makeComputeBindGroupDesc(sourceResource->view,
                                                           sourceResource->sampler,
                                                           resource.environment_face_views[face]));

        const glm::vec4 constants{static_cast<float>(face), 0.0f, 0.0f, 0.0f};
        m_compute_cmd->setBindGroup(0, computeBindGroup);
        m_compute_cmd->pushConstants(rhi::shaderStageFlag(rhi::ShaderStage::Compute), 0, sizeof(constants), &constants);
        m_compute_cmd->dispatch(environmentGroupCount, environmentGroupCount);
    }

    const rhi::TextureTransition environmentCopyDst{
        .texture = resource.environment.texture,
        .state = rhi::ResourceState::CopyDst,
    };
    m_compute_cmd->transition(&environmentCopyDst, 1);
    m_compute_cmd->generateMipmaps(resource.environment.texture);
    const rhi::TextureTransition environmentRead{
        .texture = resource.environment.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&environmentRead, 1);

    const uint32_t irradianceGroupCount =
        (resource.irradiance_size + EnvironmentComputeGroupSize - 1) / EnvironmentComputeGroupSize;
    m_compute_cmd->setPipeline(m_environment_irradiance_pipeline);

    const rhi::TextureTransition irradianceStorage{
        .texture = resource.irradiance.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&irradianceStorage, 1);

    for (uint32_t face = 0; face < 6; ++face) {
        m_device->updateBindGroup(computeBindGroup,
                                  makeComputeBindGroupDesc(resource.environment.view,
                                                           resource.environment.sampler,
                                                           resource.irradiance_face_views[face]));

        const glm::vec4 constants{static_cast<float>(face), 0.0f, 0.0f, 0.0f};
        m_compute_cmd->setBindGroup(0, computeBindGroup);
        m_compute_cmd->pushConstants(rhi::shaderStageFlag(rhi::ShaderStage::Compute), 0, sizeof(constants), &constants);
        m_compute_cmd->dispatch(irradianceGroupCount, irradianceGroupCount);
    }

    const rhi::TextureTransition irradianceRead{
        .texture = resource.irradiance.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&irradianceRead, 1);

    m_compute_cmd->setPipeline(m_environment_prefilter_pipeline);
    const rhi::TextureTransition prefilterStorage{
        .texture = resource.prefilter.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&prefilterStorage, 1);

    const uint32_t maxPrefilterMip = lastMipLevel(resource.prefilter_mip_count);
    for (uint32_t mip = 0; mip < resource.prefilter_mip_count; ++mip) {
        const uint32_t mipSize = mipDimension(resource.prefilter_size, mip);
        const uint32_t groupCount = (mipSize + EnvironmentComputeGroupSize - 1) / EnvironmentComputeGroupSize;
        const float roughness =
            maxPrefilterMip > 0 ? std::min(static_cast<float>(mip) / static_cast<float>(maxPrefilterMip), 1.0f) : 0.0f;
        const uint32_t sampleCount = prefilterSampleCount(mip, maxPrefilterMip);

        for (uint32_t face = 0; face < 6; ++face) {
            const size_t index = static_cast<size_t>(mip) * 6 + face;
            m_device->updateBindGroup(computeBindGroup,
                                      makeComputeBindGroupDesc(resource.environment.view,
                                                               resource.environment.sampler,
                                                               resource.prefilter_face_views[index]));

            const glm::vec4 constants{static_cast<float>(face), roughness, static_cast<float>(sampleCount), 0.0f};
            m_compute_cmd->setBindGroup(0, computeBindGroup);
            m_compute_cmd->pushConstants(
                rhi::shaderStageFlag(rhi::ShaderStage::Compute), 0, sizeof(constants), &constants);
            m_compute_cmd->dispatch(groupCount, groupCount);
        }
    }

    const rhi::TextureTransition prefilterRead{
        .texture = resource.prefilter.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&prefilterRead, 1);
    m_compute_cmd->end();
    m_device->submit(m_compute_command_list);
    m_device->destroyBindGroup(computeBindGroup);

    LUNA_CORE_DEBUG("Generated environment GPU resource for asset {} (cube: {}px/{} mips, irradiance: {}px, prefilter: "
                    "{}px/{} mips)",
                    handle.toString(),
                    resource.environment_size,
                    resource.environment_mip_count,
                    resource.irradiance_size,
                    resource.prefilter_size,
                    resource.prefilter_mip_count);
    return &m_environment_gpu_cache.emplace(handle, std::move(resource)).first->second;
}

void EnvironmentMapCache::updateBindGroup(const EnvironmentGpuResource& resource)
{
    if (!m_environment_bind_group || !resource.environment.view || !resource.environment.sampler ||
        !resource.irradiance.view || !resource.irradiance.sampler || !resource.prefilter.view ||
        !resource.prefilter.sampler || !m_brdf_lut_resource.view || !m_brdf_lut_resource.sampler) {
        return;
    }

    m_device->updateBindGroup(
        m_environment_bind_group,
        rhi::BindGroupDesc{
            .layout = m_environment_bind_group_layout,
            .entries =
                {
                    combinedTextureEntry(0, resource.environment.view, resource.environment.sampler),
                    combinedTextureEntry(1, resource.irradiance.view, resource.irradiance.sampler),
                    combinedTextureEntry(2, resource.prefilter.view, resource.prefilter.sampler),
                    combinedTextureEntry(3, m_brdf_lut_resource.view, m_brdf_lut_resource.sampler),
                },
        });
}

void EnvironmentMapCache::createBlackEnvironmentGpuResource()
{
    constexpr float blackPixel[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const auto makeBlackHalfPixels = [](uint32_t size) {
        constexpr uint16_t halfFloatOne = 0x3c'00u;
        std::vector<uint16_t> pixels(static_cast<size_t>(size) * size * 4, 0u);
        for (size_t texel = 0; texel < static_cast<size_t>(size) * size; ++texel) {
            pixels[texel * 4 + 3] = halfFloatOne;
        }
        return pixels;
    };

    m_black_environment_gpu_resource.environment_size = 1;
    m_black_environment_gpu_resource.environment_mip_count = 1;
    m_black_environment_gpu_resource.environment.texture = createCubeTexture(
        *m_device, 1, 1, EnvironmentCubeFormat, rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst);
    m_black_environment_gpu_resource.environment.view = createCubeTextureView(
        *m_device, m_black_environment_gpu_resource.environment.texture, EnvironmentCubeFormat, 1);
    m_black_environment_gpu_resource.environment.sampler = createCubeSampler(*m_device, 1);
    if (m_black_environment_gpu_resource.environment.texture) {
        for (uint32_t face = 0; face < 6; ++face) {
            rhi_upload::uploadTextureData(*m_device,
                                          m_upload_command_list,
                                          m_black_environment_gpu_resource.environment.texture,
                                          rhi_upload::TextureUploadData{
                                              .data = blackPixel,
                                              .size = sizeof(blackPixel),
                                              .row_pitch = sizeof(blackPixel),
                                              .width = 1,
                                              .height = 1,
                                              .array_layer = face,
                                          });
        }
    }

    m_black_environment_gpu_resource.irradiance.texture =
        createCubeTexture(*m_device,
                          DefaultIrradianceCubeSize,
                          1,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst);
    m_black_environment_gpu_resource.irradiance.view = createCubeTextureView(
        *m_device, m_black_environment_gpu_resource.irradiance.texture, FilteredEnvironmentCubeFormat, 1);
    m_black_environment_gpu_resource.irradiance.sampler = createCubeSampler(*m_device, 1);
    if (m_black_environment_gpu_resource.irradiance.texture) {
        for (uint32_t face = 0; face < 6; ++face) {
            const uint32_t size = DefaultIrradianceCubeSize;
            const auto pixels = makeBlackHalfPixels(size);
            rhi_upload::uploadTextureData(*m_device,
                                          m_upload_command_list,
                                          m_black_environment_gpu_resource.irradiance.texture,
                                          rhi_upload::TextureUploadData{
                                              .data = pixels.data(),
                                              .size = pixels.size() * sizeof(uint16_t),
                                              .row_pitch = static_cast<size_t>(size) * sizeof(uint16_t) * 4,
                                              .width = size,
                                              .height = size,
                                              .array_layer = face,
                                          });
        }
    }

    const uint32_t prefilterMipLevels = fullMipLevelCount(DefaultPrefilterCubeSize, DefaultPrefilterCubeSize);
    m_black_environment_gpu_resource.prefilter.texture =
        createCubeTexture(*m_device,
                          DefaultPrefilterCubeSize,
                          prefilterMipLevels,
                          FilteredEnvironmentCubeFormat,
                          rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage | rhi::TextureUsage::CopyDst);
    m_black_environment_gpu_resource.prefilter.view =
        createCubeTextureView(*m_device,
                              m_black_environment_gpu_resource.prefilter.texture,
                              FilteredEnvironmentCubeFormat,
                              prefilterMipLevels);
    m_black_environment_gpu_resource.prefilter.sampler = createCubeSampler(*m_device, prefilterMipLevels);
    for (uint32_t face = 0; face < 6; ++face) {
        m_black_environment_gpu_resource.environment_face_views[face] = createCubeFaceView(
            *m_device, m_black_environment_gpu_resource.environment.texture, EnvironmentCubeFormat, 0, face);
    }
    m_black_environment_gpu_resource.irradiance_size = DefaultIrradianceCubeSize;
    m_black_environment_gpu_resource.prefilter_size = DefaultPrefilterCubeSize;
    m_black_environment_gpu_resource.prefilter_mip_count = prefilterMipLevels;
    if (m_black_environment_gpu_resource.prefilter.texture) {
        for (uint32_t mip = 0; mip < prefilterMipLevels; ++mip) {
            const uint32_t mipSize = std::max(DefaultPrefilterCubeSize >> mip, 1u);
            const auto pixels = makeBlackHalfPixels(mipSize);
            for (uint32_t face = 0; face < 6; ++face) {
                rhi_upload::uploadTextureData(*m_device,
                                              m_upload_command_list,
                                              m_black_environment_gpu_resource.prefilter.texture,
                                              rhi_upload::TextureUploadData{
                                                  .data = pixels.data(),
                                                  .size = pixels.size() * sizeof(uint16_t),
                                                  .row_pitch = static_cast<size_t>(mipSize) * sizeof(uint16_t) * 4,
                                                  .width = mipSize,
                                                  .height = mipSize,
                                                  .mip_level = mip,
                                                  .array_layer = face,
                                              });
            }
        }
    }
}

void EnvironmentMapCache::createBrdfLutResource()
{
    m_brdf_lut_resource.texture = m_device->createTexture(rhi::TextureDesc{
        .width = BrdfLutSize,
        .height = BrdfLutSize,
        .dimension = rhi::TextureDimension::Texture2D,
        .format = rhi::TextureFormat::RG16F,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
        .mip_levels = 1,
        .array_layers = 1,
    });
    m_brdf_lut_resource.view = m_device->createTextureView(rhi::TextureViewDesc{
        .texture = m_brdf_lut_resource.texture,
        .view_dimension = rhi::TextureViewDimension::Texture2D,
        .format = rhi::TextureFormat::RG16F,
        .aspect = rhi::TextureAspect::Color,
        .mip_level_count = 1,
        .array_layer_count = 1,
    });
    m_brdf_lut_resource.sampler = m_device->createSampler(rhi::SamplerDesc{
        .min_filter = rhi::FilterMode::Linear,
        .mag_filter = rhi::FilterMode::Linear,
        .mip_filter = rhi::MipFilter::None,
        .address_u = rhi::AddressMode::ClampToEdge,
        .address_v = rhi::AddressMode::ClampToEdge,
        .address_w = rhi::AddressMode::ClampToEdge,
    });

    const auto computeShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Compute,
        .source = BrdfLutComputeShader,
    });
    const auto computeBindGroupLayout = m_device->createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::StorageTexture,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Compute),
                },
            },
    });
    const auto computePipelineLayout = m_device->createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {computeBindGroupLayout},
    });
    const auto computeBindGroup = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = computeBindGroupLayout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::StorageTexture,
                    .texture_view = m_brdf_lut_resource.view,
                },
            },
    });
    const auto computePipeline = m_device->createComputePipeline(rhi::ComputePipelineDesc{
        .layout = computePipelineLayout,
        .compute_shader = computeShader,
    });

    const auto destroyComputeResources = [&]() {
        if (computePipeline) {
            m_device->destroyPipeline(computePipeline);
        }
        if (computeBindGroup) {
            m_device->destroyBindGroup(computeBindGroup);
        }
        if (computePipelineLayout) {
            m_device->destroyPipelineLayout(computePipelineLayout);
        }
        if (computeBindGroupLayout) {
            m_device->destroyBindGroupLayout(computeBindGroupLayout);
        }
        if (computeShader) {
            m_device->destroyShader(computeShader);
        }
    };

    if (!m_brdf_lut_resource.texture || !m_brdf_lut_resource.view || !m_brdf_lut_resource.sampler || !computeShader ||
        !computeBindGroupLayout || !computePipelineLayout || !computeBindGroup || !computePipeline) {
        LUNA_CORE_ERROR("Failed to create BRDF LUT compute resources.");
        destroyComputeResources();
        destroyTextureGpuResource(*m_device, m_brdf_lut_resource);
        return;
    }

    m_compute_cmd->begin();
    const rhi::TextureTransition brdfLutStorageTransition{
        .texture = m_brdf_lut_resource.texture,
        .state = rhi::ResourceState::StorageReadWrite,
    };
    m_compute_cmd->transition(&brdfLutStorageTransition, 1);
    m_compute_cmd->setPipeline(computePipeline);
    m_compute_cmd->setBindGroup(0, computeBindGroup);
    m_compute_cmd->dispatch((BrdfLutSize + BrdfLutComputeGroupSize - 1) / BrdfLutComputeGroupSize,
                            (BrdfLutSize + BrdfLutComputeGroupSize - 1) / BrdfLutComputeGroupSize);
    const rhi::TextureTransition brdfLutShaderReadTransition{
        .texture = m_brdf_lut_resource.texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_compute_cmd->transition(&brdfLutShaderReadTransition, 1);
    m_compute_cmd->end();
    m_device->submit(m_compute_command_list);

    destroyComputeResources();
}

} // namespace lunalite::renderer
