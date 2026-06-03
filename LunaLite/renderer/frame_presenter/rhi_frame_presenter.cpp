#include "../../core/log.h"
#include "rhi_frame_presenter.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace lunalite::renderer {
namespace {

rhi::TextureFormat toRHITextureFormat(interface::FrameImageFormat format)
{
    switch (format) {
        case interface::FrameImageFormat::RGBA8_UNorm:
            return rhi::TextureFormat::RGBA8_UNorm;
        case interface::FrameImageFormat::RGBA16_Float:
            return rhi::TextureFormat::RGBA16F;
        case interface::FrameImageFormat::RGBA32_Float:
            return rhi::TextureFormat::RGBA32F;
    }

    return rhi::TextureFormat::RGBA8_UNorm;
}

size_t frameImageBytesPerPixel(interface::FrameImageFormat format)
{
    switch (format) {
        case interface::FrameImageFormat::RGBA8_UNorm:
            return 4;
        case interface::FrameImageFormat::RGBA16_Float:
            return 8;
        case interface::FrameImageFormat::RGBA32_Float:
            return 16;
    }

    return 4;
}

std::optional<std::string> readTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string loadFramePresenterShader(const char* file_name)
{
    const auto relative_path =
        std::filesystem::path{"LunaLite"} / "renderer" / "frame_presenter" / "shaders" / file_name;
#ifdef LUNALITE_SOURCE_ROOT
    const auto shader_path = std::filesystem::path{LUNALITE_SOURCE_ROOT} / relative_path;
#else
    const auto shader_path = relative_path;
#endif

    if (auto source = readTextFile(shader_path)) {
        return *source;
    }

    throw std::runtime_error("Failed to load frame presenter shader '" + std::string{file_name} + "' from '" +
                             shader_path.string() + "'.");
}

} // namespace

RHIFramePresenter::RHIFramePresenter(rhi::Device& device, rhi::SwapchainHandle swapchain_handle)
    : m_device(device),
      m_swapchain_handle(swapchain_handle)
{
    m_command_list = m_device.createCommandList();
    m_cmd = m_device.getCommandList(m_command_list);
    LUNA_ASSERT(m_command_list, "Failed to create presenter command list.");
    LUNA_ASSERT(m_cmd != nullptr, "RHI command list is null.");

    const auto vertexShaderSource = loadFramePresenterShader("presenter.vert");
    const auto fragmentShaderSource = loadFramePresenterShader("presenter.frag");

    m_vertex_shader = m_device.createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = vertexShaderSource.c_str(),
    });

    m_fragment_shader = m_device.createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = fragmentShaderSource.c_str(),
    });

    m_bind_group_layout = m_device.createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
            },
    });

    m_pipeline_layout = m_device.createPipelineLayout(rhi::PipelineLayoutDesc{
        .bind_group_layouts = {m_bind_group_layout},
    });

    m_pipeline = m_device.createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_input = rhi::VertexInputDesc{},
        .layout = m_pipeline_layout,
        .vertex_shader = m_vertex_shader,
        .fragment_shader = m_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets =
                    {
                        rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8_UNorm},
                    },
            },
        .depth_state =
            rhi::DepthState{
                .enabled = false,
                .write_enabled = false,
            },
    });

    m_sampler = m_device.createSampler(rhi::SamplerDesc{
        .min_filter = rhi::FilterMode::Linear,
        .mag_filter = rhi::FilterMode::Linear,
        .address_u = rhi::AddressMode::ClampToEdge,
        .address_v = rhi::AddressMode::ClampToEdge,
    });

    LUNA_ASSERT(m_vertex_shader, "Failed to create presenter vertex shader.");
    LUNA_ASSERT(m_fragment_shader, "Failed to create presenter fragment shader.");
    LUNA_ASSERT(m_bind_group_layout, "Failed to create presenter bind group layout.");
    LUNA_ASSERT(m_pipeline_layout, "Failed to create presenter pipeline layout.");
    LUNA_ASSERT(m_pipeline, "Failed to create presenter pipeline.");
    LUNA_ASSERT(m_sampler, "Failed to create presenter sampler.");
    LUNA_CORE_DEBUG("RHI frame presenter initialized");
}

RHIFramePresenter::~RHIFramePresenter()
{
    if (m_bind_group) {
        m_device.destroyBindGroup(m_bind_group);
    }
    if (m_upload_view) {
        m_device.destroyTextureView(m_upload_view);
    }
    if (m_upload_texture) {
        m_device.destroyTexture(m_upload_texture);
    }
    if (m_sampler) {
        m_device.destroySampler(m_sampler);
    }
    if (m_pipeline) {
        m_device.destroyPipeline(m_pipeline);
    }
    if (m_pipeline_layout) {
        m_device.destroyPipelineLayout(m_pipeline_layout);
    }
    if (m_bind_group_layout) {
        m_device.destroyBindGroupLayout(m_bind_group_layout);
    }
    if (m_fragment_shader) {
        m_device.destroyShader(m_fragment_shader);
    }
    if (m_vertex_shader) {
        m_device.destroyShader(m_vertex_shader);
    }
    if (m_command_list) {
        m_device.destroyCommandList(m_command_list);
        m_command_list = {};
        m_cmd = nullptr;
    }
}

void RHIFramePresenter::renderToSwapchain(const interface::FrameImage& image, const rhi::SwapchainFrame& frame)
{
    if (image.width == 0 || image.height == 0) {
        return;
    }

    if (const auto* cpu = std::get_if<interface::CpuFrameStorage>(&image.storage)) {
        uploadCpuImage(image, *cpu);
        drawToSwapchain(m_upload_view, frame);
    } else if (const auto* gpu = std::get_if<interface::GpuFrameStorage>(&image.storage)) {
        drawToSwapchain(gpu->view, frame);
    }
}

void RHIFramePresenter::present(const interface::FrameImage& image)
{
    rhi::SwapchainFrame frame{};
    if (!m_device.beginFrame(m_swapchain_handle, frame)) {
        return;
    }

    renderToSwapchain(image, frame);
    m_device.present(frame);
}

void RHIFramePresenter::ensureUploadTexture(const interface::FrameImage& image)
{
    if (m_upload_texture && m_upload_width == image.width && m_upload_height == image.height &&
        m_upload_format == image.format) {
        return;
    }

    if (m_bind_group) {
        m_device.destroyBindGroup(m_bind_group);
        m_bind_group = {};
    }
    if (m_upload_view) {
        m_device.destroyTextureView(m_upload_view);
        m_upload_view = {};
    }
    if (m_upload_texture) {
        m_device.destroyTexture(m_upload_texture);
        m_upload_texture = {};
    }

    const auto rhiFormat = toRHITextureFormat(image.format);
    m_upload_texture = m_device.createTexture(rhi::TextureDesc{
        .width = image.width,
        .height = image.height,
        .format = rhiFormat,
        .usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst,
    });
    m_upload_view = m_device.createTextureView(rhi::TextureViewDesc{
        .texture = m_upload_texture,
        .format = rhiFormat,
        .aspect = rhi::TextureAspect::Color,
    });
    LUNA_ASSERT(m_upload_texture, "Failed to create presenter upload texture.");
    LUNA_ASSERT(m_upload_view, "Failed to create presenter upload texture view.");

    m_upload_width = image.width;
    m_upload_height = image.height;
    m_upload_format = image.format;
}

void RHIFramePresenter::uploadCpuImage(const interface::FrameImage& image, const interface::CpuFrameStorage& storage)
{
    if (storage.pixels == nullptr) {
        LUNA_CORE_WARN("Skipped CPU frame upload because pixel storage is null");
        return;
    }

    ensureUploadTexture(image);
    if (!m_upload_texture) {
        return;
    }

    m_device.updateTexture(m_upload_texture,
                           rhi::TextureUploadDesc{
                               .width = image.width,
                               .height = image.height,
                               .format = toRHITextureFormat(image.format),
                               .data = storage.pixels,
                               .row_pitch = storage.row_pitch == 0 ? static_cast<size_t>(image.width) *
                                                                         frameImageBytesPerPixel(image.format)
                                                                   : storage.row_pitch,
                           });
}

void RHIFramePresenter::drawToSwapchain(rhi::TextureViewHandle view, const rhi::SwapchainFrame& frame)
{
    if (!view || !m_pipeline || !m_sampler || !m_bind_group_layout) {
        return;
    }

    const rhi::BindGroupDesc bindGroupDesc{
        .layout = m_bind_group_layout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .texture_view = view,
                    .sampler = m_sampler,
                },
            },
    };

    if (!m_bind_group) {
        m_bind_group = m_device.createBindGroup(bindGroupDesc);
    } else {
        m_device.updateBindGroup(m_bind_group, bindGroupDesc);
    }

    rhi::RenderPassBeginInfo pass;
    pass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = frame.color_view,
        .load_op = rhi::LoadOp::Clear,
        .store_op = rhi::StoreOp::Store,
        .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
    });
    pass.width = frame.width;
    pass.height = frame.height;

    m_cmd->begin();
    m_cmd->beginRenderPass(pass);
    m_cmd->setPipeline(m_pipeline);
    m_cmd->setBindGroup(0, m_bind_group);
    m_cmd->draw(3);
    m_cmd->endRenderPass();
    m_cmd->end();
    m_device.submit(m_command_list, &frame);
}

} // namespace lunalite::renderer
