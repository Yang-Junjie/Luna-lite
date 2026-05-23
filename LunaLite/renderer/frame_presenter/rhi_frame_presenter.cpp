#include "rhi_frame_presenter.h"

#include <variant>

namespace lunalite::renderer {
namespace {

struct alignas(16) PresenterUniforms {
    int32_t source_is_srgb{0};
    int32_t _padding0{0};
    int32_t _padding1{0};
    int32_t _padding2{0};
};

rhi::TextureFormat toRHITextureFormat(interface::FrameImageFormat format)
{
    switch (format) {
        case interface::FrameImageFormat::RGBA8_UNorm:
            return rhi::TextureFormat::RGBA8;
        case interface::FrameImageFormat::RGBA16_Float:
            return rhi::TextureFormat::RGBA16F;
        case interface::FrameImageFormat::RGBA32_Float:
            return rhi::TextureFormat::RGBA32F;
    }

    return rhi::TextureFormat::RGBA8;
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

constexpr const char* kPresenterVertexShaderSource = R"(
#version 450 core

out vec2 vUV;

void main()
{
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    const vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );

    vUV = uvs[gl_VertexID];
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)";

constexpr const char* kPresenterFragmentShaderSource = R"(
#version 450 core

layout(std140, binding = 0) uniform PresenterUniforms {
    int sourceIsSRGB;
    int _padding0;
    int _padding1;
    int _padding2;
};

layout(binding = 1) uniform sampler2D uFrameTexture;

in vec2 vUV;
out vec4 outColor;

void main()
{
    vec4 color = texture(uFrameTexture, vUV);
    if (sourceIsSRGB == 0) {
        color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(1.0 / 2.2));
    }
    outColor = vec4(color.rgb, color.a);
}
)";

} // namespace

RHIFramePresenter::RHIFramePresenter(rhi::Device& device, rhi::Swapchain& swapchain)
    : m_device(device),
      m_swapchain(swapchain),
      m_cmd(&m_device.getCommandList())
{
    m_vertex_shader = m_device.createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = kPresenterVertexShaderSource,
    });

    m_fragment_shader = m_device.createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = kPresenterFragmentShaderSource,
    });

    m_bind_group_layout = m_device.createBindGroupLayout(rhi::BindGroupLayoutDesc{
        .entries =
            {
                rhi::BindGroupLayoutEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .stages = rhi::shaderStageFlag(rhi::ShaderStage::Fragment),
                },
                rhi::BindGroupLayoutEntry{
                    .binding = 1,
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
        .vertex_layout = rhi::VertexLayoutDesc{},
        .layout = m_pipeline_layout,
        .vertex_shader = m_vertex_shader,
        .fragment_shader = m_fragment_shader,
        .render_target_state =
            rhi::RenderTargetState{
                .color_targets =
                    {
                        rhi::ColorTargetState{.format = rhi::TextureFormat::RGBA8},
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

    m_uniform_buffer = m_device.createBuffer(
        rhi::BufferDesc{
            .type = rhi::BufferType::UniformBuffer,
            .usage = rhi::BufferUsage::Dynamic,
            .size = sizeof(PresenterUniforms),
        },
        nullptr);
}

RHIFramePresenter::~RHIFramePresenter()
{
    if (m_bind_group != 0) {
        m_device.destroyBindGroup(m_bind_group);
    }
    if (m_upload_view != 0) {
        m_device.destroyTextureView(m_upload_view);
    }
    if (m_upload_texture != 0) {
        m_device.destroyTexture(m_upload_texture);
    }
    if (m_sampler != 0) {
        m_device.destroySampler(m_sampler);
    }
    if (m_uniform_buffer != 0) {
        m_device.destroyBuffer(m_uniform_buffer);
    }
    if (m_pipeline != 0) {
        m_device.destroyPipeline(m_pipeline);
    }
    if (m_pipeline_layout != 0) {
        m_device.destroyPipelineLayout(m_pipeline_layout);
    }
    if (m_bind_group_layout != 0) {
        m_device.destroyBindGroupLayout(m_bind_group_layout);
    }
    if (m_fragment_shader != 0) {
        m_device.destroyShader(m_fragment_shader);
    }
    if (m_vertex_shader != 0) {
        m_device.destroyShader(m_vertex_shader);
    }
}

void RHIFramePresenter::present(const interface::FrameImage& image)
{
    if (image.width == 0 || image.height == 0) {
        return;
    }

    if (const auto* cpu = std::get_if<interface::CpuFrameStorage>(&image.storage)) {
        uploadCpuImage(image, *cpu);
        drawToSwapchain(m_upload_view, image.color_space);
    } else if (const auto* gpu = std::get_if<interface::GpuFrameStorage>(&image.storage)) {
        drawToSwapchain(gpu->view, image.color_space);
    }

    m_swapchain.present();
}

void RHIFramePresenter::ensureUploadTexture(const interface::FrameImage& image)
{
    if (m_upload_texture != 0 && m_upload_width == image.width && m_upload_height == image.height &&
        m_upload_format == image.format) {
        return;
    }

    if (m_bind_group != 0) {
        m_device.destroyBindGroup(m_bind_group);
        m_bind_group = 0;
    }
    if (m_upload_view != 0) {
        m_device.destroyTextureView(m_upload_view);
        m_upload_view = 0;
    }
    if (m_upload_texture != 0) {
        m_device.destroyTexture(m_upload_texture);
        m_upload_texture = 0;
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

    m_upload_width = image.width;
    m_upload_height = image.height;
    m_upload_format = image.format;
}

void RHIFramePresenter::uploadCpuImage(const interface::FrameImage& image, const interface::CpuFrameStorage& storage)
{
    if (storage.pixels == nullptr) {
        return;
    }

    ensureUploadTexture(image);
    if (m_upload_texture == 0) {
        return;
    }

    m_device.updateTexture(m_upload_texture,
                           rhi::TextureUploadDesc{
                               .width = image.width,
                               .height = image.height,
                               .format = toRHITextureFormat(image.format),
                               .data = storage.pixels,
                               .row_pitch = storage.row_pitch == 0
                                                ? static_cast<size_t>(image.width) * frameImageBytesPerPixel(image.format)
                                                : storage.row_pitch,
                           });
}

void RHIFramePresenter::drawToSwapchain(rhi::TextureViewHandle view, interface::FrameImageColorSpace color_space)
{
    if (view == 0 || m_pipeline == 0 || m_sampler == 0 || m_uniform_buffer == 0 || m_bind_group_layout == 0) {
        return;
    }

    const PresenterUniforms uniforms{
        .source_is_srgb = color_space == interface::FrameImageColorSpace::SRGB ? 1 : 0,
    };
    m_device.updateBuffer(m_uniform_buffer, &uniforms, sizeof(uniforms));

    const rhi::BindGroupDesc bindGroupDesc{
        .layout = m_bind_group_layout,
        .entries =
            {
                rhi::BindGroupEntry{
                    .binding = 0,
                    .type = rhi::BindingType::UniformBuffer,
                    .buffer =
                        rhi::BufferBinding{
                            .buffer = m_uniform_buffer,
                            .offset = 0,
                            .size = sizeof(PresenterUniforms),
                        },
                },
                rhi::BindGroupEntry{
                    .binding = 1,
                    .type = rhi::BindingType::CombinedImageSampler,
                    .texture_view = view,
                    .sampler = m_sampler,
                },
            },
    };

    if (m_bind_group == 0) {
        m_bind_group = m_device.createBindGroup(bindGroupDesc);
    } else {
        m_device.updateBindGroup(m_bind_group, bindGroupDesc);
    }

    rhi::RenderPassBeginInfo pass;
    pass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = m_swapchain.getCurrentColorTextureView(),
        .load_op = rhi::LoadOp::Clear,
        .store_op = rhi::StoreOp::Store,
        .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
    });
    pass.width = m_swapchain.getWidth();
    pass.height = m_swapchain.getHeight();

    m_cmd->begin();
    m_cmd->beginRenderPass(pass);
    m_cmd->setPipeline(m_pipeline);
    m_cmd->setBindGroup(0, m_bind_group);
    m_cmd->draw(3);
    m_cmd->endRenderPass();
    m_cmd->end();
}

} // namespace lunalite::renderer
