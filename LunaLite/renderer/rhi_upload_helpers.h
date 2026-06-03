#pragma once

#include "TinyRHI/interface/device.h"

#include <cstddef>
#include <cstdint>

namespace lunalite::renderer::rhi_upload {

struct TextureUploadData {
    const void* data{nullptr};
    size_t size{0};
    size_t row_pitch{0};
    uint32_t x{0};
    uint32_t y{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t mip_level{0};
    uint32_t array_layer{0};
};

inline bool uploadTextureRegions(rhi::Device& device,
                                 rhi::CommandListHandle command_list,
                                 rhi::TextureHandle texture,
                                 const void* data,
                                 size_t size,
                                 const rhi::BufferTextureCopyRegion* regions,
                                 uint32_t region_count,
                                 bool generate_mipmaps)
{
    if (!texture || data == nullptr || size == 0 || regions == nullptr || region_count == 0) {
        return false;
    }

    const auto staging = device.createBuffer(rhi::BufferDesc{
        .size = size,
        .usage = rhi::BufferUsage::CopySrc,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::CopySrc,
    });
    auto* commands = device.getCommandList(command_list);
    if (!staging || commands == nullptr) {
        if (staging) {
            device.destroyBuffer(staging);
        }
        return false;
    }

    device.updateBuffer(staging, 0, data, size);

    const rhi::TextureTransition copyDst{
        .texture = texture,
        .state = rhi::ResourceState::CopyDst,
    };
    const rhi::TextureTransition shaderRead{
        .texture = texture,
        .state = rhi::ResourceState::ShaderRead,
    };

    commands->begin();
    commands->transition(&copyDst, 1);
    commands->copyBufferToTexture(staging, texture, regions, region_count);
    if (generate_mipmaps) {
        commands->generateMipmaps(texture);
    }
    commands->transition(&shaderRead, 1);
    commands->end();
    device.submit(command_list);

    device.destroyBuffer(staging);
    return true;
}

inline bool uploadTextureData(rhi::Device& device,
                              rhi::CommandListHandle command_list,
                              rhi::TextureHandle texture,
                              const TextureUploadData& upload,
                              bool generate_mipmaps = false)
{
    if (upload.data == nullptr || upload.size == 0 || upload.width == 0 || upload.height == 0) {
        return false;
    }

    const rhi::BufferTextureCopyRegion region{
        .buffer_offset = 0,
        .buffer_row_pitch = upload.row_pitch,
        .texture_x = upload.x,
        .texture_y = upload.y,
        .texture_width = upload.width,
        .texture_height = upload.height,
        .mip_level = upload.mip_level,
        .array_layer = upload.array_layer,
    };
    return uploadTextureRegions(device, command_list, texture, upload.data, upload.size, &region, 1, generate_mipmaps);
}

} // namespace lunalite::renderer::rhi_upload
