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

struct PipelineDesc {
    PrimitiveTopology topology;
    VertexLayoutDesc vertex_layout;
    ShaderHandle vertex_shader;
    ShaderHandle fragment_shader;
};

} // namespace lunalite::rhi
