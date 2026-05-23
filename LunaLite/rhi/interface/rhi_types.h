#pragma once

#include <cstddef>
#include <cstdint>

#include <vector>

namespace lunalite::rhi {

using BufferHandle = uint32_t;
using ShaderHandle = uint32_t;
using PipelineHandle = uint32_t;
using TextureHandle = uint32_t;
using TextureViewHandle = uint32_t;

enum class BackendType {
    OpenGL,
    Vulkan,
    D3D12,
    Metal
};

struct WindowRequirements {
    BackendType backend = BackendType::OpenGL;

    // 只有 OpenGL 会用
    int glMajor = 4;
    int glMinor = 5;
    bool gl_core_profile = true;
    bool gl_debug_context = true;
};

struct WindowHandle {
    void* native_window = nullptr;
};

enum class ShaderStage {
    Vertex,
    Fragment
    // Compute
};

enum class BufferType {
    VertexBuffer,
    IndexBuffer,
    UniformBuffer
};

enum class BufferUsage {
    Static,
    Dynamic
};

enum class PrimitiveTopology {
    Triangle,
    Line,
    Point
};

enum class LoadOp {
    Load,
    Clear,
    DontCare
};

enum class StoreOp {
    Store,
    DontCare
};

enum class VertexFormat {
    Float1,
    Float2,
    Float3,
    Float4,
    Int1,
    Int2,
    Int3,
    Int4,
    UInt1,
    UInt2,
    UInt3,
    UInt4,
    Bool1,
    Bool2,
    Bool3,
    Bool4,
    Byte
};

enum class VertexAttribute {
    Position,
    Normal,
    TexCoord,
    Color
};

struct VertexAttributeDesc {
    VertexAttribute semantic;
    VertexFormat format;
    uint32_t offset;
};

struct VertexLayoutDesc {
    uint32_t stride;
    std::vector<VertexAttributeDesc> attributes;
};

struct BufferDesc {
    BufferType type;
    BufferUsage usage;
    size_t size;
};

struct ShaderDesc {
    ShaderStage stage;
    const char* source;
};

struct PipelineDesc {
    PrimitiveTopology topology;
    VertexLayoutDesc vertex_layout;
    ShaderHandle vertex_shader;
    ShaderHandle fragment_shader;
};

struct ClearColor {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
};

struct ColorAttachmentDesc {
    TextureViewHandle view{0};
    LoadOp load_op{LoadOp::Clear};
    StoreOp store_op{StoreOp::Store};
    ClearColor clear_color{};
};

struct DepthStencilAttachmentDesc {
    TextureViewHandle view{0};

    LoadOp depth_load_op{LoadOp::Clear};
    StoreOp depth_store_op{StoreOp::DontCare};
    float clear_depth{1.0f};

    // LoadOp stencil_load_op{LoadOp::DontCare};
    // StoreOp stencil_store_op{StoreOp::DontCare};
    // uint8_t clear_stencil{0};
};

struct RenderPassBeginInfo {
    std::vector<ColorAttachmentDesc> color_attachments;
    bool has_depth_stencil_attachment{false};
    DepthStencilAttachmentDesc depth_stencil_attachment{};
    uint32_t width{0};
    uint32_t height{0};
};

} // namespace lunalite::rhi
