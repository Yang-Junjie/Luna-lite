#pragma once
#include "rhi_types.h"

namespace lunalite::rhi {

enum class ShaderStage {
    Vertex,
    Fragment
    // Compute
};

struct ShaderDesc {
    ShaderStage stage;
    const char* source;
};

} // namespace lunalite::rhi
