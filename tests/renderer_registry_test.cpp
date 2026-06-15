#include "../LunaLite/renderer/interface/frame_render_data.h"
#include "../LunaLite/renderer/interface/renderer.h"
#include "../LunaLite/renderer/renderer_controller.h"
#include "../LunaLite/renderer/renderer_registry.h"
#include "TinyRHI/interface/device.h"

#include <cstddef>

#include <iostream>
#include <memory>

namespace {
using namespace lunalite;

class FakeSwapchain : public rhi::Swapchain {
public:
    uint32_t getWidth() const override
    {
        return m_width;
    }

    uint32_t getHeight() const override
    {
        return m_height;
    }

    void resize(uint32_t width, uint32_t height) override
    {
        m_width = width;
        m_height = height;
        resize_count += 1;
    }

    int resize_count{0};

private:
    uint32_t m_width{640};
    uint32_t m_height{480};
};

class FakeDevice : public rhi::Device {
public:
    rhi::BufferHandle createBuffer(const rhi::BufferDesc&) override
    {
        return {};
    }

    void updateBuffer(rhi::BufferHandle, size_t, const void*, size_t) override {}

    void destroyBuffer(rhi::BufferHandle) override {}

    rhi::TextureHandle createTexture(const rhi::TextureDesc&) override
    {
        return {};
    }

    void destroyTexture(rhi::TextureHandle) override {}

    rhi::TextureViewHandle createTextureView(const rhi::TextureViewDesc&) override
    {
        return {};
    }

    void destroyTextureView(rhi::TextureViewHandle) override {}

    rhi::SamplerHandle createSampler(const rhi::SamplerDesc&) override
    {
        return {};
    }

    void destroySampler(rhi::SamplerHandle) override {}

    rhi::BindGroupLayoutHandle createBindGroupLayout(const rhi::BindGroupLayoutDesc&) override
    {
        return {};
    }

    void destroyBindGroupLayout(rhi::BindGroupLayoutHandle) override {}

    rhi::BindGroupHandle createBindGroup(const rhi::BindGroupDesc&) override
    {
        return {};
    }

    void updateBindGroup(rhi::BindGroupHandle, const rhi::BindGroupDesc&) override {}

    void destroyBindGroup(rhi::BindGroupHandle) override {}

    rhi::PipelineLayoutHandle createPipelineLayout(const rhi::PipelineLayoutDesc&) override
    {
        return {};
    }

    void destroyPipelineLayout(rhi::PipelineLayoutHandle) override {}

    rhi::ShaderHandle createShader(const rhi::ShaderDesc&) override
    {
        return {};
    }

    void destroyShader(rhi::ShaderHandle) override {}

    rhi::PipelineHandle createPipeline(const rhi::PipelineDesc&) override
    {
        return {};
    }

    rhi::PipelineHandle createComputePipeline(const rhi::ComputePipelineDesc&) override
    {
        return {};
    }

    void destroyPipeline(rhi::PipelineHandle) override {}

    rhi::TimestampQueryPoolHandle createTimestampQueryPool(const rhi::TimestampQueryPoolDesc&) override
    {
        return {};
    }

    void destroyTimestampQueryPool(rhi::TimestampQueryPoolHandle) override {}

    bool getTimestampQueryResults(rhi::TimestampQueryPoolHandle, uint32_t, uint32_t, uint64_t*) override
    {
        return false;
    }

    rhi::SwapchainHandle createSwapchain(rhi::SurfaceHandle, const rhi::SwapchainDesc&) override
    {
        return {};
    }

    void destroySwapchain(rhi::SwapchainHandle) override {}

    rhi::Swapchain* getSwapchain(rhi::SwapchainHandle) override
    {
        return nullptr;
    }

    bool beginFrame(rhi::SwapchainHandle, rhi::SwapchainFrame&) override
    {
        return false;
    }

    void present(const rhi::SwapchainFrame&) override {}

    rhi::CommandListHandle createCommandList() override
    {
        return {};
    }

    void destroyCommandList(rhi::CommandListHandle) override {}

    rhi::CommandList* getCommandList(rhi::CommandListHandle) override
    {
        return nullptr;
    }

    void submit(rhi::CommandListHandle, const rhi::SwapchainFrame*) override {}
};

class FakeRenderer : public renderer::interface::Renderer {
public:
    FakeRenderer(uint32_t width, uint32_t height)
    {
        m_frame_image.width = width;
        m_frame_image.height = height;
    }

    void beginFrame() override
    {
        begin_count += 1;
    }

    void endFrame() override
    {
        end_count += 1;
    }

    void resize(uint32_t width, uint32_t height) override
    {
        m_frame_image.width = width;
        m_frame_image.height = height;
        resize_count += 1;
    }

    void renderFrame(const renderer::interface::FrameRenderData&) override
    {
        render_count += 1;
    }

    const renderer::interface::FrameImage& getFrameImage() const override
    {
        return m_frame_image;
    }

    const diagnostics::RenderStats& getStats() const override
    {
        return m_stats;
    }

    int begin_count{0};
    int end_count{0};
    int render_count{0};
    int resize_count{0};

private:
    renderer::interface::FrameImage m_frame_image;
    diagnostics::RenderStats m_stats;
};

int created_count = 0;
uint32_t created_width = 0;
uint32_t created_height = 0;

std::unique_ptr<renderer::interface::Renderer> createFakeRenderer(const renderer::RendererCreateInfo& info)
{
    created_count += 1;
    created_width = info.width;
    created_height = info.height;
    return std::make_unique<FakeRenderer>(info.width, info.height);
}

} // namespace

int main()
{
    using namespace lunalite;

    auto& registry = renderer::RendererRegistry::get();
    const bool registered = registry.registerRenderer({
        .kind = renderer::interface::RendererKind::Default,
        .display_name = "Fake Test Renderer",
        .create = &createFakeRenderer,
    });
    if (!registered) {
        std::cerr << "Fake renderer should register in a fresh test process.\n";
        return 1;
    }

    const bool duplicate_registered = registry.registerRenderer({
        .kind = renderer::interface::RendererKind::Default,
        .display_name = "Duplicate Fake Test Renderer",
        .create = &createFakeRenderer,
    });
    if (duplicate_registered) {
        std::cerr << "RendererRegistry should reject duplicate renderer kinds.\n";
        return 1;
    }

    FakeDevice device;
    FakeSwapchain swapchain;
    renderer::RendererController controller(device, swapchain, 320, 180, renderer::interface::RendererKind::Default);

    if (created_count != 1 || created_width != 320 || created_height != 180) {
        std::cerr << "RendererController did not create the registered renderer with the requested size.\n";
        return 1;
    }

    if (controller.getKind() != renderer::interface::RendererKind::Default) {
        std::cerr << "RendererController did not store the active renderer kind.\n";
        return 1;
    }

    auto* fake_renderer = dynamic_cast<FakeRenderer*>(&controller.getRenderer());
    if (fake_renderer == nullptr) {
        std::cerr << "RendererController did not use the registry factory.\n";
        return 1;
    }

    renderer::interface::FrameRenderData frame;
    fake_renderer->beginFrame();
    fake_renderer->renderFrame(frame);
    fake_renderer->endFrame();
    if (fake_renderer->begin_count != 1 || fake_renderer->render_count != 1 || fake_renderer->end_count != 1) {
        std::cerr << "Fake renderer lifecycle counters were not updated.\n";
        return 1;
    }

    controller.resize(800, 600);
    if (swapchain.resize_count != 1 || fake_renderer->resize_count != 1) {
        std::cerr << "RendererController resize should resize both swapchain and renderer.\n";
        return 1;
    }
    if (controller.getFrameImage().width != 800 || controller.getFrameImage().height != 600) {
        std::cerr << "RendererController did not expose the resized frame image.\n";
        return 1;
    }

    controller.switchRenderer(renderer::interface::RendererKind::Default);
    if (created_count != 1) {
        std::cerr << "Switching to the active renderer kind should be a no-op.\n";
        return 1;
    }

    std::cout << "RendererRegistry creates registered renderers through RendererController.\n";
    return 0;
}
