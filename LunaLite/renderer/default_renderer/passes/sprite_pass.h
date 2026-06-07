#pragma once
#include "../../interface/frame_render_data.h"
#include "../renderer_common.h"
#include "../texture_gpu_cache.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/device.h"

#include <span>
#include <unordered_map>
#include <vector>

namespace lunalite::renderer {

class SpritePass {
public:
    SpritePass(rhi::Device& device,
               rhi::CommandList& commands,
               rhi::PipelineHandle sprite_depth_pipeline,
               rhi::PipelineHandle sprite_overlay_pipeline,
               rhi::BindGroupLayoutHandle sprite_texture_bind_group_layout,
               rhi::BindGroupHandle geometry_bind_group,
               rhi::BufferHandle frame_uniform_buffer,
               TextureGpuCache& texture_cache,
               FrameUniforms& frame_uniforms,
               bool& frame_uniforms_dirty);
    ~SpritePass();

    SpritePass(const SpritePass&) = delete;
    SpritePass& operator=(const SpritePass&) = delete;

    uint32_t execute(const GBuffer& gbuffer, std::span<const interface::SpriteDrawCommand> sprite_commands);

private:
    struct SortedSprite {
        const interface::SpriteDrawCommand* command{nullptr};
        size_t original_index{0};
    };

    struct SpriteTextureKey {
        asset::AssetHandle texture{0};
        interface::TextureColorSpace color_space{interface::TextureColorSpace::SRGB};

        bool operator==(const SpriteTextureKey& other) const
        {
            return texture == other.texture && color_space == other.color_space;
        }
    };

    struct SpriteTextureKeyHash {
        size_t operator()(const SpriteTextureKey& key) const noexcept;
    };

    void flushFrameUniforms();
    void ensureBuffers(size_t vertex_count, size_t index_count);
    rhi::BindGroupHandle textureBindGroup(const interface::SpriteDrawCommand& sprite);
    uint32_t renderDepthModeBatch(const GBuffer& gbuffer, std::span<const SortedSprite> sprites, bool depth_test);
    uint32_t renderTextureBatch(const GBuffer& gbuffer,
                                rhi::PipelineHandle pipeline,
                                std::span<const SortedSprite> sprites,
                                bool depth_test);

    rhi::Device* m_device{nullptr};
    rhi::CommandList* m_cmd{nullptr};
    rhi::PipelineHandle m_sprite_depth_pipeline{};
    rhi::PipelineHandle m_sprite_overlay_pipeline{};
    rhi::BindGroupLayoutHandle m_sprite_texture_bind_group_layout{};
    rhi::BindGroupHandle m_geometry_bind_group{};
    rhi::BufferHandle m_frame_uniform_buffer{};
    rhi::BufferHandle m_sprite_vertex_buffer{};
    rhi::BufferHandle m_sprite_index_buffer{};
    size_t m_sprite_vertex_buffer_capacity{0};
    size_t m_sprite_index_buffer_capacity{0};
    TextureGpuCache* m_texture_cache{nullptr};
    FrameUniforms* m_frame_uniforms{nullptr};
    bool* m_frame_uniforms_dirty{nullptr};
    std::vector<SortedSprite> m_sorted_sprites;
    std::vector<SpriteVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::unordered_map<SpriteTextureKey, rhi::BindGroupHandle, SpriteTextureKeyHash> m_texture_bind_groups;
};

} // namespace lunalite::renderer
