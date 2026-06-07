#include "../../../core/log.h"
#include "sprite_pass.h"

#include <algorithm>
#include <array>
#include <limits>

namespace lunalite::renderer {
namespace {
glm::vec3 transformPoint(const glm::mat4& transform, const glm::vec3& point)
{
    return glm::vec3{transform * glm::vec4{point, 1.0f}};
}

void appendSpriteGeometry(const interface::SpriteDrawCommand& sprite,
                          std::vector<SpriteVertex>& vertices,
                          std::vector<uint32_t>& indices)
{
    LUNA_ASSERT(vertices.size() <= std::numeric_limits<uint32_t>::max(), "Too many sprite vertices.");
    const auto baseVertex = static_cast<uint32_t>(vertices.size());

    const auto u0 = sprite.uv_rect.x;
    const auto v0 = sprite.uv_rect.y;
    const auto u1 = sprite.uv_rect.x + sprite.uv_rect.z;
    const auto v1 = sprite.uv_rect.y + sprite.uv_rect.w;

    const std::array<glm::vec3, 4> positions = {
        glm::vec3{-0.5f, -0.5f, 0.0f},
        glm::vec3{0.5f, -0.5f, 0.0f},
        glm::vec3{0.5f, 0.5f, 0.0f},
        glm::vec3{-0.5f, 0.5f, 0.0f},
    };
    const std::array<glm::vec2, 4> uvs = {
        glm::vec2{u0, v1},
        glm::vec2{u1, v1},
        glm::vec2{u1, v0},
        glm::vec2{u0, v0},
    };

    for (size_t i = 0; i < positions.size(); ++i) {
        vertices.push_back(SpriteVertex{
            .position = transformPoint(sprite.transform, positions[i]),
            .uv = uvs[i],
            .color = sprite.color,
        });
    }

    indices.push_back(baseVertex + 0u);
    indices.push_back(baseVertex + 1u);
    indices.push_back(baseVertex + 2u);
    indices.push_back(baseVertex + 2u);
    indices.push_back(baseVertex + 3u);
    indices.push_back(baseVertex + 0u);
}
} // namespace

SpritePass::SpritePass(rhi::Device& device,
                       rhi::CommandList& commands,
                       rhi::PipelineHandle sprite_depth_pipeline,
                       rhi::PipelineHandle sprite_overlay_pipeline,
                       rhi::BindGroupLayoutHandle sprite_texture_bind_group_layout,
                       rhi::BindGroupHandle geometry_bind_group,
                       rhi::BufferHandle frame_uniform_buffer,
                       TextureGpuCache& texture_cache,
                       FrameUniforms& frame_uniforms,
                       bool& frame_uniforms_dirty)
    : m_device(&device),
      m_cmd(&commands),
      m_sprite_depth_pipeline(sprite_depth_pipeline),
      m_sprite_overlay_pipeline(sprite_overlay_pipeline),
      m_sprite_texture_bind_group_layout(sprite_texture_bind_group_layout),
      m_geometry_bind_group(geometry_bind_group),
      m_frame_uniform_buffer(frame_uniform_buffer),
      m_texture_cache(&texture_cache),
      m_frame_uniforms(&frame_uniforms),
      m_frame_uniforms_dirty(&frame_uniforms_dirty)
{
    m_sprite_vertex_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(SpriteVertex) * 4,
        .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::VertexBuffer,
    });
    m_sprite_index_buffer = m_device->createBuffer(rhi::BufferDesc{
        .size = sizeof(uint32_t) * 6,
        .usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst,
        .memory = rhi::MemoryUsage::CpuToGpu,
        .initial_state = rhi::ResourceState::IndexBuffer,
    });
    m_sprite_vertex_buffer_capacity = sizeof(SpriteVertex) * 4;
    m_sprite_index_buffer_capacity = sizeof(uint32_t) * 6;

    LUNA_ASSERT(m_sprite_vertex_buffer, "Failed to create sprite vertex buffer.");
    LUNA_ASSERT(m_sprite_index_buffer, "Failed to create sprite index buffer.");
}

SpritePass::~SpritePass()
{
    for (const auto& [_, bindGroup] : m_texture_bind_groups) {
        if (bindGroup) {
            m_device->destroyBindGroup(bindGroup);
        }
    }
    m_texture_bind_groups.clear();

    if (m_sprite_index_buffer) {
        m_device->destroyBuffer(m_sprite_index_buffer);
        m_sprite_index_buffer = {};
    }
    if (m_sprite_vertex_buffer) {
        m_device->destroyBuffer(m_sprite_vertex_buffer);
        m_sprite_vertex_buffer = {};
    }
}

uint32_t SpritePass::execute(const GBuffer& gbuffer, std::span<const interface::SpriteDrawCommand> sprite_commands)
{
    if (sprite_commands.empty() || gbuffer.width == 0 || gbuffer.height == 0 || !gbuffer.final_color_view) {
        return 0;
    }

    m_sorted_sprites.clear();
    m_sorted_sprites.reserve(sprite_commands.size());
    for (size_t i = 0; i < sprite_commands.size(); ++i) {
        m_sorted_sprites.push_back(SortedSprite{.command = &sprite_commands[i], .original_index = i});
    }

    std::ranges::stable_sort(m_sorted_sprites, [](const SortedSprite& lhs, const SortedSprite& rhs) {
        const auto& a = *lhs.command;
        const auto& b = *rhs.command;
        if (a.depth_test != b.depth_test) {
            return a.depth_test && !b.depth_test;
        }
        if (a.sorting_layer != b.sorting_layer) {
            return a.sorting_layer < b.sorting_layer;
        }
        if (a.order_in_layer != b.order_in_layer) {
            return a.order_in_layer < b.order_in_layer;
        }
        if (a.texture != b.texture) {
            return static_cast<uint64_t>(a.texture) < static_cast<uint64_t>(b.texture);
        }
        return lhs.original_index < rhs.original_index;
    });

    flushFrameUniforms();

    uint32_t drawCalls = 0;
    size_t depthBegin = 0;
    while (depthBegin < m_sorted_sprites.size() && m_sorted_sprites[depthBegin].command->depth_test) {
        ++depthBegin;
    }

    if (depthBegin > 0) {
        drawCalls +=
            renderDepthModeBatch(gbuffer, std::span<const SortedSprite>{m_sorted_sprites.data(), depthBegin}, true);
    }
    if (depthBegin < m_sorted_sprites.size()) {
        drawCalls += renderDepthModeBatch(gbuffer,
                                          std::span<const SortedSprite>{
                                              m_sorted_sprites.data() + depthBegin,
                                              m_sorted_sprites.size() - depthBegin,
                                          },
                                          false);
    }
    return drawCalls;
}

void SpritePass::flushFrameUniforms()
{
    if (!*m_frame_uniforms_dirty) {
        return;
    }

    m_device->updateBuffer(m_frame_uniform_buffer, 0, m_frame_uniforms, sizeof(FrameUniforms));
    *m_frame_uniforms_dirty = false;
}

void SpritePass::ensureBuffers(size_t vertex_count, size_t index_count)
{
    const auto requiredVertexSize = vertex_count * sizeof(SpriteVertex);
    if (requiredVertexSize > 0 && m_sprite_vertex_buffer_capacity < requiredVertexSize) {
        if (m_sprite_vertex_buffer) {
            m_device->destroyBuffer(m_sprite_vertex_buffer);
            m_sprite_vertex_buffer = {};
        }

        m_sprite_vertex_buffer_capacity = growBufferCapacity(m_sprite_vertex_buffer_capacity, requiredVertexSize);
        m_sprite_vertex_buffer = m_device->createBuffer(rhi::BufferDesc{
            .size = m_sprite_vertex_buffer_capacity,
            .usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst,
            .memory = rhi::MemoryUsage::CpuToGpu,
            .initial_state = rhi::ResourceState::VertexBuffer,
        });
        LUNA_ASSERT(m_sprite_vertex_buffer, "Failed to grow sprite vertex buffer.");
    }

    const auto requiredIndexSize = index_count * sizeof(uint32_t);
    if (requiredIndexSize > 0 && m_sprite_index_buffer_capacity < requiredIndexSize) {
        if (m_sprite_index_buffer) {
            m_device->destroyBuffer(m_sprite_index_buffer);
            m_sprite_index_buffer = {};
        }

        m_sprite_index_buffer_capacity = growBufferCapacity(m_sprite_index_buffer_capacity, requiredIndexSize);
        m_sprite_index_buffer = m_device->createBuffer(rhi::BufferDesc{
            .size = m_sprite_index_buffer_capacity,
            .usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst,
            .memory = rhi::MemoryUsage::CpuToGpu,
            .initial_state = rhi::ResourceState::IndexBuffer,
        });
        LUNA_ASSERT(m_sprite_index_buffer, "Failed to grow sprite index buffer.");
    }
}

rhi::BindGroupHandle SpritePass::textureBindGroup(asset::AssetHandle texture)
{
    const auto& resource = m_texture_cache->getOrCreate(texture, FallbackTexture::White);
    const auto cacheKey =
        texture.isValid() && m_texture_cache->find(texture) != nullptr ? texture : asset::AssetHandle{0};
    if (const auto it = m_texture_bind_groups.find(cacheKey); it != m_texture_bind_groups.end()) {
        return it->second;
    }

    auto bindGroup = m_device->createBindGroup(rhi::BindGroupDesc{
        .layout = m_sprite_texture_bind_group_layout,
        .entries = {combinedTextureEntry(0, resource.view, resource.sampler)},
    });
    LUNA_ASSERT(bindGroup, "Failed to create sprite texture bind group.");
    return m_texture_bind_groups.emplace(cacheKey, bindGroup).first->second;
}

uint32_t
    SpritePass::renderDepthModeBatch(const GBuffer& gbuffer, std::span<const SortedSprite> sprites, bool depth_test)
{
    if (sprites.empty()) {
        return 0;
    }

    uint32_t drawCalls = 0;
    size_t batchBegin = 0;
    while (batchBegin < sprites.size()) {
        const auto texture = sprites[batchBegin].command->texture;
        size_t batchEnd = batchBegin + 1;
        while (batchEnd < sprites.size() && sprites[batchEnd].command->texture == texture) {
            ++batchEnd;
        }

        drawCalls += renderTextureBatch(gbuffer,
                                        depth_test ? m_sprite_depth_pipeline : m_sprite_overlay_pipeline,
                                        sprites.subspan(batchBegin, batchEnd - batchBegin),
                                        depth_test);
        batchBegin = batchEnd;
    }

    return drawCalls;
}

uint32_t SpritePass::renderTextureBatch(const GBuffer& gbuffer,
                                        rhi::PipelineHandle pipeline,
                                        std::span<const SortedSprite> sprites,
                                        bool depth_test)
{
    if (sprites.empty() || !pipeline) {
        return 0;
    }
    LUNA_ASSERT(sprites.size() <= std::numeric_limits<uint32_t>::max() / 4u, "Too many sprites in a batch.");

    m_vertices.clear();
    m_indices.clear();
    m_vertices.reserve(sprites.size() * 4);
    m_indices.reserve(sprites.size() * 6);
    for (const auto& sprite : sprites) {
        appendSpriteGeometry(*sprite.command, m_vertices, m_indices);
    }

    ensureBuffers(m_vertices.size(), m_indices.size());
    m_device->updateBuffer(m_sprite_vertex_buffer, 0, m_vertices.data(), m_vertices.size() * sizeof(SpriteVertex));
    m_device->updateBuffer(m_sprite_index_buffer, 0, m_indices.data(), m_indices.size() * sizeof(uint32_t));

    const rhi::TextureTransition colorBarrier{
        .texture = gbuffer.final_color_texture,
        .state = rhi::ResourceState::ColorAttachment,
    };
    m_cmd->transition(&colorBarrier, 1);

    if (depth_test && gbuffer.depth_texture) {
        const rhi::TextureTransition depthBarrier{
            .texture = gbuffer.depth_texture,
            .state = rhi::ResourceState::DepthStencilWrite,
        };
        m_cmd->transition(&depthBarrier, 1);
    }

    rhi::RenderPassBeginInfo pass;
    pass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = gbuffer.final_color_view,
        .load_op = rhi::LoadOp::Load,
        .store_op = rhi::StoreOp::Store,
    });
    if (depth_test && gbuffer.depth_view) {
        pass.has_depth_stencil_attachment = true;
        pass.depth_stencil_attachment = rhi::DepthStencilAttachmentDesc{
            .view = gbuffer.depth_view,
            .depth_load_op = rhi::LoadOp::Load,
            .depth_store_op = rhi::StoreOp::Store,
        };
    }
    pass.width = gbuffer.width;
    pass.height = gbuffer.height;

    m_cmd->beginRenderPass(pass);
    m_cmd->setPipeline(pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);
    m_cmd->setBindGroup(1, textureBindGroup(sprites.front().command->texture));
    m_cmd->setVertexBuffer(0, m_sprite_vertex_buffer);
    m_cmd->setIndexBuffer(m_sprite_index_buffer, rhi::IndexFormat::UInt32);
    m_cmd->drawIndexed(static_cast<uint32_t>(m_indices.size()));
    m_cmd->endRenderPass();
    return 1;
}

} // namespace lunalite::renderer
