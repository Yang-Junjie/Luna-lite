#include "imgui_platform.h"
#include "imgui_renderer.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/render_pass.h"

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace lunalite::rhi;

namespace lunalite::imgui {
namespace {

constexpr size_t kInitialVertexBufferSize = 5'000 * sizeof(ImDrawVert);
constexpr size_t kInitialIndexBufferSize = 10'000 * sizeof(ImDrawIdx);
constexpr uint32_t kProjectionMatrixSize = 16 * sizeof(float);

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

std::string loadImGuiShader(const char* file_name)
{
    const auto relative_path = std::filesystem::path{"LunaLite"} / "imgui" / "shaders" / file_name;
#ifdef LUNALITE_SOURCE_ROOT
    const auto shader_path = std::filesystem::path{LUNALITE_SOURCE_ROOT} / relative_path;
#else
    const auto shader_path = relative_path;
#endif

    if (auto source = readTextFile(shader_path)) {
        return *source;
    }

    throw std::runtime_error("Failed to load ImGui shader '" + std::string{file_name} + "' from '" +
                             shader_path.string() + "'.");
}

ImTextureID textureIdFromBindGroup(BindGroupHandle bind_group)
{
    return static_cast<ImTextureID>(handleValue(bind_group));
}

struct ViewportRenderData {
    SurfaceHandle surface{};
    SwapchainHandle swapchain_handle{};
    Swapchain* swapchain{nullptr};
    SwapchainFrame frame{};
    bool frame_pending{false};
};

uint32_t viewportDimension(float value)
{
    return value > 0.0f ? static_cast<uint32_t>(value) : 1;
}

ViewportRenderData* viewportRenderData(ImGuiViewport* viewport)
{
    return viewport != nullptr ? static_cast<ViewportRenderData*>(viewport->RendererUserData) : nullptr;
}

IndexFormat imguiIndexFormat()
{
    static_assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4);
    return sizeof(ImDrawIdx) == 2 ? IndexFormat::UInt16 : IndexFormat::UInt32;
}

TextureFormat toRHITextureFormat(renderer::interface::FrameImageFormat format)
{
    switch (format) {
        case renderer::interface::FrameImageFormat::RGBA8_UNorm:
            return TextureFormat::RGBA8_UNorm;
        case renderer::interface::FrameImageFormat::RGBA16_Float:
            return TextureFormat::RGBA16F;
        case renderer::interface::FrameImageFormat::RGBA32_Float:
            return TextureFormat::RGBA32F;
    }

    return TextureFormat::RGBA8_UNorm;
}

size_t frameImageBytesPerPixel(renderer::interface::FrameImageFormat format)
{
    switch (format) {
        case renderer::interface::FrameImageFormat::RGBA8_UNorm:
            return 4;
        case renderer::interface::FrameImageFormat::RGBA16_Float:
            return 8;
        case renderer::interface::FrameImageFormat::RGBA32_Float:
            return 16;
    }

    return 4;
}

} // namespace

ImGuiRenderer::~ImGuiRenderer()
{
    shutdown();
}

bool ImGuiRenderer::init(Device& device, SwapchainHandle swapchain_handle, Swapchain& swapchain)
{
    if (m_device != nullptr) {
        return false;
    }

    m_device = &device;
    m_swapchain_handle = swapchain_handle;
    m_swapchain = &swapchain;
    m_command_list = m_device->createCommandList();
    if (!m_command_list || m_device->getCommandList(m_command_list) == nullptr) {
        m_device = nullptr;
        m_swapchain_handle = {};
        m_swapchain = nullptr;
        m_command_list = {};
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "LunaLite TinyRHI";
    io.BackendRendererUserData = this;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasViewports;
    initViewportSupport();

    if (!createStaticResources() || !createFontTexture()) {
        shutdown();
        return false;
    }

    return true;
}

void ImGuiRenderer::setSurfaceOwner(Instance& instance)
{
    m_instance = &instance;
}

void ImGuiRenderer::setPlatform(ImGuiPlatform& platform)
{
    m_platform = &platform;
}

void ImGuiRenderer::beginFrame()
{
    if (m_platform == nullptr) {
        return;
    }

    m_platform->newFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::endFrame(ImGuiRenderMode mode, const SwapchainFrame& frame)
{
    if (m_device == nullptr || m_swapchain == nullptr || !m_command_list) {
        return;
    }

    ImGui::Render();

    RenderPassBeginInfo pass;
    pass.color_attachments.push_back(ColorAttachmentDesc{
        .view = frame.color_view,
        .load_op = mode == ImGuiRenderMode::ClearSwapchain ? LoadOp::Clear : LoadOp::Load,
        .store_op = StoreOp::Store,
        .clear_color = ClearColor{0.08f, 0.09f, 0.11f, 1.0f},
    });
    pass.width = frame.width;
    pass.height = frame.height;

    auto* commandList = m_device->getCommandList(m_command_list);
    if (commandList == nullptr) {
        return;
    }
    auto& commands = *commandList;
    commands.begin();
    commands.beginRenderPass(pass);
    render(ImGui::GetDrawData(), commands);
    commands.endRenderPass();
    commands.end();
    m_device->submit(m_command_list, &frame);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, this);
    }

    m_device->present(frame);
}

void ImGuiRenderer::shutdown()
{
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        shutdownViewportSupport();
        io.BackendRendererName = nullptr;
        io.BackendRendererUserData = nullptr;
        io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasViewports);
        io.Fonts->SetTexID(ImTextureID_Invalid);
    }

    if (m_device == nullptr) {
        m_instance = nullptr;
        return;
    }

    destroyBuffers();
    destroyFontTexture();
    destroyUploadTexture();
    destroyExternalBindGroups();

    if (m_pipeline) {
        m_device->destroyPipeline(m_pipeline);
        m_pipeline = {};
    }
    if (m_pipeline_layout) {
        m_device->destroyPipelineLayout(m_pipeline_layout);
        m_pipeline_layout = {};
    }
    if (m_bind_group_layout) {
        m_device->destroyBindGroupLayout(m_bind_group_layout);
        m_bind_group_layout = {};
    }
    if (m_sampler) {
        m_device->destroySampler(m_sampler);
        m_sampler = {};
    }
    if (m_fragment_shader) {
        m_device->destroyShader(m_fragment_shader);
        m_fragment_shader = {};
    }
    if (m_vertex_shader) {
        m_device->destroyShader(m_vertex_shader);
        m_vertex_shader = {};
    }
    if (m_command_list) {
        m_device->destroyCommandList(m_command_list);
        m_command_list = {};
    }

    m_device = nullptr;
    m_swapchain_handle = {};
    m_swapchain = nullptr;
    m_instance = nullptr;
    m_platform = nullptr;
}

void ImGuiRenderer::render(ImDrawData* draw_data, CommandList& commands)
{
    if (m_device == nullptr || draw_data == nullptr || draw_data->TotalVtxCount == 0) {
        return;
    }

    const int framebufferWidth = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    const int framebufferHeight = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    if (!ensureBuffers(draw_data->TotalVtxCount, draw_data->TotalIdxCount)) {
        return;
    }

    m_vertex_upload.resize(static_cast<size_t>(draw_data->TotalVtxCount));
    m_index_upload.resize(static_cast<size_t>(draw_data->TotalIdxCount));

    ImDrawVert* vertexDst = m_vertex_upload.data();
    ImDrawIdx* indexDst = m_index_upload.data();
    for (int listIndex = 0; listIndex < draw_data->CmdListsCount; ++listIndex) {
        const ImDrawList* commandList = draw_data->CmdLists[listIndex];
        const size_t vertexBytes = static_cast<size_t>(commandList->VtxBuffer.Size) * sizeof(ImDrawVert);
        const size_t indexBytes = static_cast<size_t>(commandList->IdxBuffer.Size) * sizeof(ImDrawIdx);
        std::memcpy(vertexDst, commandList->VtxBuffer.Data, vertexBytes);
        std::memcpy(indexDst, commandList->IdxBuffer.Data, indexBytes);
        vertexDst += commandList->VtxBuffer.Size;
        indexDst += commandList->IdxBuffer.Size;
    }

    m_device->updateBuffer(m_vertex_buffer, 0, m_vertex_upload.data(), m_vertex_upload.size() * sizeof(ImDrawVert));
    m_device->updateBuffer(m_index_buffer, 0, m_index_upload.data(), m_index_upload.size() * sizeof(ImDrawIdx));

    const float left = draw_data->DisplayPos.x;
    const float right = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float top = draw_data->DisplayPos.y;
    const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float projection[4][4] = {
        {2.0f / (right - left), 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / (top - bottom), 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {(right + left) / (left - right), (top + bottom) / (bottom - top), 0.0f, 1.0f},
    };

    Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(framebufferWidth),
        .height = static_cast<float>(framebufferHeight),
        .min_depth = 0.0f,
        .max_depth = 1.0f,
    };

    commands.setPipeline(m_pipeline);
    commands.setViewport(0, &viewport, 1);
    commands.pushConstants(shaderStageFlag(ShaderStage::Vertex), 0, sizeof(projection), projection);
    commands.setVertexBuffer(0, m_vertex_buffer);
    commands.setIndexBuffer(m_index_buffer, imguiIndexFormat());

    const ImVec2 clipOffset = draw_data->DisplayPos;
    const ImVec2 clipScale = draw_data->FramebufferScale;
    uint32_t globalIndexOffset = 0;
    int32_t globalVertexOffset = 0;

    for (int listIndex = 0; listIndex < draw_data->CmdListsCount; ++listIndex) {
        const ImDrawList* commandList = draw_data->CmdLists[listIndex];
        for (int commandIndex = 0; commandIndex < commandList->CmdBuffer.Size; ++commandIndex) {
            const ImDrawCmd* drawCommand = &commandList->CmdBuffer[commandIndex];
            if (drawCommand->UserCallback != nullptr) {
                drawCommand->UserCallback(commandList, drawCommand);
                continue;
            }

            const float clipMinX = std::max((drawCommand->ClipRect.x - clipOffset.x) * clipScale.x, 0.0f);
            const float clipMinY = std::max((drawCommand->ClipRect.y - clipOffset.y) * clipScale.y, 0.0f);
            const float clipMaxX =
                std::min((drawCommand->ClipRect.z - clipOffset.x) * clipScale.x, static_cast<float>(framebufferWidth));
            const float clipMaxY =
                std::min((drawCommand->ClipRect.w - clipOffset.y) * clipScale.y, static_cast<float>(framebufferHeight));

            if (clipMaxX <= clipMinX || clipMaxY <= clipMinY) {
                continue;
            }

            ScissorRect scissor{
                .x = static_cast<int32_t>(clipMinX),
                .y = static_cast<int32_t>(static_cast<float>(framebufferHeight) - clipMaxY),
                .width = static_cast<uint32_t>(clipMaxX - clipMinX),
                .height = static_cast<uint32_t>(clipMaxY - clipMinY),
            };

            const BindGroupHandle bindGroup = bindGroupFromTextureId(drawCommand->GetTexID());
            if (!bindGroup) {
                continue;
            }

            commands.setScissor(0, &scissor, 1);
            commands.setBindGroup(0, bindGroup);
            commands.drawIndexed(drawCommand->ElemCount,
                                 drawCommand->IdxOffset + globalIndexOffset,
                                 static_cast<int32_t>(drawCommand->VtxOffset) + globalVertexOffset);
        }

        globalIndexOffset += static_cast<uint32_t>(commandList->IdxBuffer.Size);
        globalVertexOffset += commandList->VtxBuffer.Size;
    }
}

ImTextureID ImGuiRenderer::textureId(const renderer::interface::FrameImage& image)
{
    if (m_device == nullptr || image.width == 0 || image.height == 0) {
        return ImTextureID_Invalid;
    }

    if (const auto* gpu = std::get_if<renderer::interface::GpuFrameStorage>(&image.storage)) {
        return textureId(gpu->view);
    }

    if (const auto* cpu = std::get_if<renderer::interface::CpuFrameStorage>(&image.storage)) {
        uploadCpuImage(image, *cpu);
        return m_upload_bind_group ? textureIdFromBindGroup(m_upload_bind_group) : ImTextureID_Invalid;
    }

    return ImTextureID_Invalid;
}

ImTextureID ImGuiRenderer::textureId(TextureViewHandle view)
{
    const auto bindGroup = bindGroupForTextureView(view);
    return bindGroup ? textureIdFromBindGroup(bindGroup) : ImTextureID_Invalid;
}

bool ImGuiRenderer::createStaticResources()
{
    const auto vertexShaderSource = loadImGuiShader("imgui.vert");
    const auto fragmentShaderSource = loadImGuiShader("imgui.frag");

    m_vertex_shader =
        m_device->createShader(ShaderDesc{.stage = ShaderStage::Vertex, .source = vertexShaderSource.c_str()});
    m_fragment_shader =
        m_device->createShader(ShaderDesc{.stage = ShaderStage::Fragment, .source = fragmentShaderSource.c_str()});
    m_sampler = m_device->createSampler(SamplerDesc{
        .min_filter = FilterMode::Linear,
        .mag_filter = FilterMode::Linear,
        .address_u = AddressMode::ClampToEdge,
        .address_v = AddressMode::ClampToEdge,
    });

    BindGroupLayoutDesc bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entries.push_back(BindGroupLayoutEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .stages = shaderStageFlag(ShaderStage::Fragment),
        .count = 1,
    });
    m_bind_group_layout = m_device->createBindGroupLayout(bindGroupLayoutDesc);

    PipelineLayoutDesc pipelineLayoutDesc{};
    pipelineLayoutDesc.bind_group_layouts.push_back(m_bind_group_layout);
    pipelineLayoutDesc.push_constants.push_back(PushConstantRange{
        .stages = shaderStageFlag(ShaderStage::Vertex),
        .offset = 0,
        .size = kProjectionMatrixSize,
    });
    m_pipeline_layout = m_device->createPipelineLayout(pipelineLayoutDesc);

    PipelineDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangle;
    pipelineDesc.vertex_input.buffers.push_back(VertexBufferLayoutDesc{
        .binding = 0,
        .stride = sizeof(ImDrawVert),
        .step_mode = VertexStepMode::Vertex,
        .attributes =
            {
                VertexAttributeDesc{
                    .location = 0,
                    .format = VertexFormat::Float2,
                    .offset = offsetof(ImDrawVert, pos),
                },
                VertexAttributeDesc{
                    .location = 1,
                    .format = VertexFormat::Float2,
                    .offset = offsetof(ImDrawVert, uv),
                },
                VertexAttributeDesc{
                    .location = 2,
                    .format = VertexFormat::RGBA8Unorm,
                    .offset = offsetof(ImDrawVert, col),
                },
            },
    });
    pipelineDesc.layout = m_pipeline_layout;
    pipelineDesc.vertex_shader = m_vertex_shader;
    pipelineDesc.fragment_shader = m_fragment_shader;
    ColorTargetState colorTarget{.format = TextureFormat::RGBA8_UNorm};
    colorTarget.blend.enabled = true;
    pipelineDesc.render_target_state.color_targets.push_back(colorTarget);
    pipelineDesc.depth_state.enabled = false;
    pipelineDesc.raster_state.cull_mode = CullMode::None;
    m_pipeline = m_device->createPipeline(pipelineDesc);

    return m_vertex_shader && m_fragment_shader && m_sampler && m_bind_group_layout && m_pipeline_layout && m_pipeline;
}

bool ImGuiRenderer::createFontTexture()
{
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    int bytesPerPixel = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);
    if (pixels == nullptr || width <= 0 || height <= 0 || bytesPerPixel != 4) {
        return false;
    }

    m_font_texture = m_device->createTexture(TextureDesc{
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .format = TextureFormat::RGBA8_UNorm,
        .usage = TextureUsage::Sampled | TextureUsage::CopyDst,
    });
    m_font_texture_view = m_device->createTextureView(TextureViewDesc{
        .texture = m_font_texture,
        .format = TextureFormat::RGBA8_UNorm,
        .aspect = TextureAspect::Color,
    });

    BindGroupDesc bindGroupDesc{};
    bindGroupDesc.layout = m_bind_group_layout;
    bindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = m_font_texture_view,
        .sampler = m_sampler,
    });
    m_font_bind_group = m_device->createBindGroup(bindGroupDesc);

    if (!m_font_texture || !m_font_texture_view || !m_font_bind_group) {
        return false;
    }

    TextureUploadDesc upload{};
    upload.width = static_cast<uint32_t>(width);
    upload.height = static_cast<uint32_t>(height);
    upload.format = TextureFormat::RGBA8_UNorm;
    upload.data = pixels;
    upload.row_pitch = static_cast<size_t>(width) * static_cast<size_t>(bytesPerPixel);
    m_device->updateTexture(m_font_texture, upload);

    ImGui::GetIO().Fonts->SetTexID(textureIdFromBindGroup(m_font_bind_group));
    return true;
}

bool ImGuiRenderer::ensureBuffers(int vertex_count, int index_count)
{
    const size_t requiredVertexBytes = static_cast<size_t>(vertex_count) * sizeof(ImDrawVert);
    const size_t requiredIndexBytes = static_cast<size_t>(index_count) * sizeof(ImDrawIdx);

    if (!m_vertex_buffer || m_vertex_buffer_size < requiredVertexBytes) {
        if (m_vertex_buffer) {
            m_device->destroyBuffer(m_vertex_buffer);
        }
        m_vertex_buffer_size = std::max(requiredVertexBytes, kInitialVertexBufferSize);
        m_vertex_buffer = m_device->createBuffer(
            BufferDesc{
                .size = m_vertex_buffer_size,
                .usage = BufferUsage::Vertex | BufferUsage::CopyDst,
                .memory = MemoryUsage::CpuToGpu,
            },
            nullptr);
    }

    if (!m_index_buffer || m_index_buffer_size < requiredIndexBytes) {
        if (m_index_buffer) {
            m_device->destroyBuffer(m_index_buffer);
        }
        m_index_buffer_size = std::max(requiredIndexBytes, kInitialIndexBufferSize);
        m_index_buffer = m_device->createBuffer(
            BufferDesc{
                .size = m_index_buffer_size,
                .usage = BufferUsage::Index | BufferUsage::CopyDst,
                .memory = MemoryUsage::CpuToGpu,
            },
            nullptr);
    }

    return m_vertex_buffer && m_index_buffer;
}

void ImGuiRenderer::ensureUploadTexture(const renderer::interface::FrameImage& image)
{
    if (m_upload_texture && m_upload_width == image.width && m_upload_height == image.height &&
        m_upload_format == image.format) {
        return;
    }

    destroyUploadTexture();

    const auto rhiFormat = toRHITextureFormat(image.format);
    m_upload_texture = m_device->createTexture(TextureDesc{
        .width = image.width,
        .height = image.height,
        .format = rhiFormat,
        .usage = TextureUsage::Sampled | TextureUsage::CopyDst,
    });
    m_upload_view = m_device->createTextureView(TextureViewDesc{
        .texture = m_upload_texture,
        .format = rhiFormat,
        .aspect = TextureAspect::Color,
    });

    if (!m_upload_texture || !m_upload_view) {
        return;
    }

    BindGroupDesc bindGroupDesc{};
    bindGroupDesc.layout = m_bind_group_layout;
    bindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = m_upload_view,
        .sampler = m_sampler,
    });
    m_upload_bind_group = m_device->createBindGroup(bindGroupDesc);

    m_upload_width = image.width;
    m_upload_height = image.height;
    m_upload_format = image.format;
}

void ImGuiRenderer::uploadCpuImage(const renderer::interface::FrameImage& image,
                                   const renderer::interface::CpuFrameStorage& storage)
{
    if (storage.pixels == nullptr) {
        return;
    }

    ensureUploadTexture(image);
    if (!m_upload_texture) {
        return;
    }

    m_device->updateTexture(m_upload_texture,
                            TextureUploadDesc{
                                .width = image.width,
                                .height = image.height,
                                .format = toRHITextureFormat(image.format),
                                .data = storage.pixels,
                                .row_pitch = storage.row_pitch == 0 ? static_cast<size_t>(image.width) *
                                                                          frameImageBytesPerPixel(image.format)
                                                                    : storage.row_pitch,
                            });
}

BindGroupHandle ImGuiRenderer::bindGroupForTextureView(TextureViewHandle view)
{
    if (!view || m_device == nullptr || !m_bind_group_layout || !m_sampler) {
        return {};
    }

    const uint32_t key = handleValue(view);
    const auto it = m_texture_bind_groups.find(key);
    if (it != m_texture_bind_groups.end()) {
        return it->second;
    }

    BindGroupDesc bindGroupDesc{};
    bindGroupDesc.layout = m_bind_group_layout;
    bindGroupDesc.entries.push_back(BindGroupEntry{
        .binding = 0,
        .type = BindingType::CombinedImageSampler,
        .texture_view = view,
        .sampler = m_sampler,
    });

    const auto bindGroup = m_device->createBindGroup(bindGroupDesc);
    if (bindGroup) {
        m_texture_bind_groups.emplace(key, bindGroup);
    }

    return bindGroup;
}

void ImGuiRenderer::initViewportSupport()
{
    ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
    platformIO.Renderer_CreateWindow = &ImGuiRenderer::createViewportWindowCallback;
    platformIO.Renderer_DestroyWindow = &ImGuiRenderer::destroyViewportWindowCallback;
    platformIO.Renderer_SetWindowSize = &ImGuiRenderer::setViewportWindowSizeCallback;
    platformIO.Renderer_RenderWindow = &ImGuiRenderer::renderViewportWindowCallback;
    platformIO.Renderer_SwapBuffers = &ImGuiRenderer::swapViewportBuffersCallback;
}

void ImGuiRenderer::shutdownViewportSupport()
{
    ImGui::DestroyPlatformWindows();
    ImGui::GetPlatformIO().ClearRendererHandlers();
}

void ImGuiRenderer::createViewportWindow(ImGuiViewport* viewport)
{
    if (m_device == nullptr || m_instance == nullptr || m_platform == nullptr || viewport == nullptr ||
        viewport->RendererUserData != nullptr) {
        return;
    }

    const auto nativeWindow = m_platform->nativeWindowHandle(*viewport);
    if (nativeWindow.platform == NativeWindowHandle::Platform::Unknown || nativeWindow.window == nullptr) {
        return;
    }

    auto* data = IM_NEW(ViewportRenderData)();
    data->surface = m_instance->createSurface(nativeWindow);
    auto* surface = m_instance->getSurface(data->surface);
    if (surface == nullptr) {
        IM_DELETE(data);
        return;
    }
    surface->resize(viewportDimension(viewport->Size.x), viewportDimension(viewport->Size.y));

    SwapchainDesc desc{};
    desc.enable_depth_stencil = false;
    data->swapchain_handle = m_device->createSwapchain(data->surface, desc);
    data->swapchain = m_device->getSwapchain(data->swapchain_handle);
    if (data->swapchain == nullptr) {
        m_instance->destroySurface(data->surface);
        IM_DELETE(data);
        return;
    }

    viewport->RendererUserData = data;
}

void ImGuiRenderer::destroyViewportWindow(ImGuiViewport* viewport)
{
    auto* data = viewportRenderData(viewport);
    if (data != nullptr) {
        if (m_device != nullptr && data->swapchain_handle) {
            m_device->destroySwapchain(data->swapchain_handle);
        }
        if (m_instance != nullptr && data->surface) {
            m_instance->destroySurface(data->surface);
        }
        data->swapchain = nullptr;
        data->swapchain_handle = {};
        data->surface = {};
        data->frame = {};
        data->frame_pending = false;
        IM_DELETE(data);
    }

    if (viewport != nullptr) {
        viewport->RendererUserData = nullptr;
    }
}

void ImGuiRenderer::setViewportWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    auto* data = viewportRenderData(viewport);
    if (data == nullptr || data->swapchain == nullptr) {
        return;
    }

    const uint32_t width = viewportDimension(size.x);
    const uint32_t height = viewportDimension(size.y);
    if (m_instance != nullptr) {
        if (auto* surface = m_instance->getSurface(data->surface)) {
            surface->resize(width, height);
        }
    }
    data->swapchain->resize(width, height);
}

void ImGuiRenderer::renderViewportWindow(ImGuiViewport* viewport)
{
    auto* data = viewportRenderData(viewport);
    if (m_device == nullptr || !m_command_list || data == nullptr || data->swapchain == nullptr ||
        viewport == nullptr || viewport->DrawData == nullptr) {
        return;
    }
    data->frame_pending = false;

    const uint32_t width = viewportDimension(viewport->Size.x);
    const uint32_t height = viewportDimension(viewport->Size.y);
    if (m_instance != nullptr) {
        if (auto* surface = m_instance->getSurface(data->surface)) {
            surface->resize(width, height);
        }
    }
    data->swapchain->resize(width, height);

    SwapchainFrame frame{};
    if (!m_device->beginFrame(data->swapchain_handle, frame)) {
        return;
    }

    RenderPassBeginInfo pass{};
    pass.color_attachments.push_back(ColorAttachmentDesc{
        .view = frame.color_view,
        .load_op = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) ? LoadOp::Load : LoadOp::Clear,
        .store_op = StoreOp::Store,
        .clear_color = ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
    });
    pass.width = frame.width;
    pass.height = frame.height;

    auto* commandList = m_device->getCommandList(m_command_list);
    if (commandList == nullptr) {
        return;
    }
    auto& commands = *commandList;
    commands.begin();
    commands.beginRenderPass(pass);
    render(viewport->DrawData, commands);
    commands.endRenderPass();
    commands.end();
    m_device->submit(m_command_list, &frame);
    data->frame = frame;
    data->frame_pending = true;
}

void ImGuiRenderer::swapViewportBuffers(ImGuiViewport* viewport)
{
    auto* data = viewportRenderData(viewport);
    if (m_device != nullptr && data != nullptr && data->swapchain != nullptr && data->frame_pending) {
        m_device->present(data->frame);
        data->frame_pending = false;
    }
}

void ImGuiRenderer::destroyBuffers()
{
    if (m_vertex_buffer) {
        m_device->destroyBuffer(m_vertex_buffer);
        m_vertex_buffer = {};
    }
    if (m_index_buffer) {
        m_device->destroyBuffer(m_index_buffer);
        m_index_buffer = {};
    }
    m_vertex_buffer_size = 0;
    m_index_buffer_size = 0;
}

void ImGuiRenderer::destroyFontTexture()
{
    if (m_font_bind_group) {
        m_device->destroyBindGroup(m_font_bind_group);
        m_font_bind_group = {};
    }
    if (m_font_texture_view) {
        m_device->destroyTextureView(m_font_texture_view);
        m_font_texture_view = {};
    }
    if (m_font_texture) {
        m_device->destroyTexture(m_font_texture);
        m_font_texture = {};
    }
}

void ImGuiRenderer::destroyUploadTexture()
{
    if (m_upload_bind_group) {
        m_device->destroyBindGroup(m_upload_bind_group);
        m_upload_bind_group = {};
    }
    if (m_upload_view) {
        m_device->destroyTextureView(m_upload_view);
        m_upload_view = {};
    }
    if (m_upload_texture) {
        m_device->destroyTexture(m_upload_texture);
        m_upload_texture = {};
    }
    m_upload_width = 0;
    m_upload_height = 0;
}

void ImGuiRenderer::destroyExternalBindGroups()
{
    for (const auto& [_, bindGroup] : m_texture_bind_groups) {
        if (bindGroup) {
            m_device->destroyBindGroup(bindGroup);
        }
    }
    m_texture_bind_groups.clear();
}

BindGroupHandle ImGuiRenderer::bindGroupFromTextureId(ImTextureID texture_id) const
{
    if (texture_id == ImTextureID_Invalid || texture_id > std::numeric_limits<uint32_t>::max()) {
        return {};
    }

    return BindGroupHandle{static_cast<uint32_t>(texture_id)};
}

ImGuiRenderer* ImGuiRenderer::currentRenderer()
{
    if (ImGui::GetCurrentContext() == nullptr) {
        return nullptr;
    }

    return static_cast<ImGuiRenderer*>(ImGui::GetIO().BackendRendererUserData);
}

void ImGuiRenderer::createViewportWindowCallback(ImGuiViewport* viewport)
{
    if (auto* renderer = currentRenderer()) {
        renderer->createViewportWindow(viewport);
    }
}

void ImGuiRenderer::destroyViewportWindowCallback(ImGuiViewport* viewport)
{
    if (auto* renderer = currentRenderer()) {
        renderer->destroyViewportWindow(viewport);
    }
}

void ImGuiRenderer::setViewportWindowSizeCallback(ImGuiViewport* viewport, ImVec2 size)
{
    if (auto* renderer = currentRenderer()) {
        renderer->setViewportWindowSize(viewport, size);
    }
}

void ImGuiRenderer::renderViewportWindowCallback(ImGuiViewport* viewport, void* render_arg)
{
    auto* renderer = render_arg != nullptr ? static_cast<ImGuiRenderer*>(render_arg) : currentRenderer();
    if (renderer != nullptr) {
        renderer->renderViewportWindow(viewport);
    }
}

void ImGuiRenderer::swapViewportBuffersCallback(ImGuiViewport* viewport, void* render_arg)
{
    auto* renderer = render_arg != nullptr ? static_cast<ImGuiRenderer*>(render_arg) : currentRenderer();
    if (renderer != nullptr) {
        renderer->swapViewportBuffers(viewport);
    }
}

} // namespace lunalite::imgui
