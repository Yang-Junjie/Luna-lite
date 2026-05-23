#pragma once
#include "rhi_types.h"

#include <cstddef>

namespace lunalite::rhi {

enum class BufferType {
    VertexBuffer,
    IndexBuffer,
    UniformBuffer
};

enum class BufferUsage {
    Static,
    Dynamic
};

struct BufferDesc {
    BufferType type;
    BufferUsage usage;
    size_t size;
};

} // namespace lunalite::rhi
