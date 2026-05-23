#pragma once
#include "rhi_types.h"

#include <cstdint>
#include <vector>

namespace lunalite::rhi {

enum class PrimitiveTopology {
    Triangle,
    Line,
    Point
};

enum class CullMode {
    None,
    Front,
    Back
};

enum class FrontFace {
    Clockwise,
    CounterClockwise
};

enum class CompareOp {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always
};

enum class BlendFactor {
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha
};

enum class BlendOp {
    Add,
    Subtract,
    ReverseSubtract
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

struct DepthState {
    bool enabled{true};
    bool write_enabled{true};
    CompareOp compare{CompareOp::Less};
};

struct RasterState {
    CullMode cull_mode{CullMode::None};
    FrontFace front_face{FrontFace::CounterClockwise};
};

struct BlendState {
    bool enabled{false};
    BlendFactor src_color{BlendFactor::SrcAlpha};
    BlendFactor dst_color{BlendFactor::OneMinusSrcAlpha};
    BlendOp color_op{BlendOp::Add};
    BlendFactor src_alpha{BlendFactor::One};
    BlendFactor dst_alpha{BlendFactor::OneMinusSrcAlpha};
    BlendOp alpha_op{BlendOp::Add};
};

struct PipelineDesc {
    PrimitiveTopology topology;
    VertexLayoutDesc vertex_layout;
    ShaderHandle vertex_shader;
    ShaderHandle fragment_shader;
    DepthState depth_state{};
    RasterState raster_state{};
    BlendState blend_state{};
};

} // namespace lunalite::rhi
