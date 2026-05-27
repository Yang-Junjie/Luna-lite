#pragma once

#include "../renderer/interface/frame_image.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/instance.h"
#include "TinyRHI/interface/swapchain.h"

#include <imgui.h>

#include <cstddef>
#include <cstdint>

#include <unordered_map>
#include <vector>

namespace lunalite::imgui {

class ImGuiPlatform;

enum class ImGuiRenderMode {
    ClearSwapchain,
    OverlaySwapchain
};

class ImGuiRenderer final {
public:
    ImGuiRenderer() = default;
    ~ImGuiRenderer();

    ImGuiRenderer(const ImGuiRenderer&) = delete;
    ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

    bool init(rhi::Device& device, rhi::Swapchain& swapchain);
    void setSurfaceOwner(rhi::Instance& instance);
    void setPlatform(ImGuiPlatform& platform);
    void beginFrame();
    void endFrame(ImGuiRenderMode mode);
    void shutdown();
    void render(ImDrawData* draw_data, rhi::CommandList& commands);
    ImTextureID textureId(const renderer::interface::FrameImage& image);
    ImTextureID textureId(rhi::TextureViewHandle view);

private:
    bool createStaticResources();
    bool createFontTexture();
    bool ensureBuffers(int vertex_count, int index_count);
    void ensureUploadTexture(const renderer::interface::FrameImage& image);
    void uploadCpuImage(const renderer::interface::FrameImage& image,
                        const renderer::interface::CpuFrameStorage& storage);
    rhi::BindGroupHandle bindGroupForTextureView(rhi::TextureViewHandle view);
    void initViewportSupport();
    void shutdownViewportSupport();
    void createViewportWindow(ImGuiViewport* viewport);
    void destroyViewportWindow(ImGuiViewport* viewport);
    void setViewportWindowSize(ImGuiViewport* viewport, ImVec2 size);
    void renderViewportWindow(ImGuiViewport* viewport);
    void swapViewportBuffers(ImGuiViewport* viewport);
    void destroyBuffers();
    void destroyFontTexture();
    void destroyUploadTexture();
    void destroyExternalBindGroups();
    rhi::BindGroupHandle bindGroupFromTextureId(ImTextureID texture_id) const;

    static ImGuiRenderer* currentRenderer();
    static void createViewportWindowCallback(ImGuiViewport* viewport);
    static void destroyViewportWindowCallback(ImGuiViewport* viewport);
    static void setViewportWindowSizeCallback(ImGuiViewport* viewport, ImVec2 size);
    static void renderViewportWindowCallback(ImGuiViewport* viewport, void* render_arg);
    static void swapViewportBuffersCallback(ImGuiViewport* viewport, void* render_arg);

    rhi::Device* m_device{nullptr};
    rhi::Swapchain* m_swapchain{nullptr};
    rhi::Instance* m_instance{nullptr};
    ImGuiPlatform* m_platform{nullptr};
    rhi::BufferHandle m_vertex_buffer{};
    rhi::BufferHandle m_index_buffer{};
    size_t m_vertex_buffer_size{0};
    size_t m_index_buffer_size{0};
    rhi::ShaderHandle m_vertex_shader{};
    rhi::ShaderHandle m_fragment_shader{};
    rhi::SamplerHandle m_sampler{};
    rhi::BindGroupLayoutHandle m_bind_group_layout{};
    rhi::PipelineLayoutHandle m_pipeline_layout{};
    rhi::PipelineHandle m_pipeline{};
    rhi::TextureHandle m_font_texture{};
    rhi::TextureViewHandle m_font_texture_view{};
    rhi::BindGroupHandle m_font_bind_group{};
    rhi::TextureHandle m_upload_texture{};
    rhi::TextureViewHandle m_upload_view{};
    rhi::BindGroupHandle m_upload_bind_group{};
    uint32_t m_upload_width{0};
    uint32_t m_upload_height{0};
    renderer::interface::FrameImageFormat m_upload_format{renderer::interface::FrameImageFormat::RGBA8_UNorm};
    std::unordered_map<uint32_t, rhi::BindGroupHandle> m_texture_bind_groups;
    std::vector<ImDrawVert> m_vertex_upload;
    std::vector<ImDrawIdx> m_index_upload;
};

} // namespace lunalite::imgui
