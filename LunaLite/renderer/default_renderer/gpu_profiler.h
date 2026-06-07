#pragma once
#include "../../diagnostics/stats.h"
#include "TinyRHI/interface/rhi_types.h"

#include <cstdint>

#include <array>

namespace lunalite::rhi {
class CommandList;
class Device;
} // namespace lunalite::rhi

namespace lunalite::renderer {

class GpuProfiler {
public:
    enum class Pass : uint32_t {
        Frame,
        Shadow,
        ShadowCascade0,
        ShadowCascade1,
        ShadowCascade2,
        ShadowCascade3,
        Geometry,
        Lighting,
        Skybox,
        DebugLines,
        Count
    };

    explicit GpuProfiler(rhi::Device& device);
    ~GpuProfiler();

    const diagnostics::GpuProfilerStats& beginFrame(rhi::CommandList& cmd);
    void beginPass(rhi::CommandList& cmd, Pass pass);
    void endPass(rhi::CommandList& cmd, Pass pass);
    void recordEmptyPass(rhi::CommandList& cmd, Pass pass);
    void endFrame(rhi::CommandList& cmd);
    static Pass shadowCascadePass(uint32_t cascade_index);

private:
    static constexpr uint32_t BufferedFrames = 4;
    static constexpr uint32_t PassCount = static_cast<uint32_t>(Pass::Count);
    static constexpr uint32_t QueriesPerPass = 2;
    static constexpr uint32_t QueryCount = PassCount * QueriesPerPass;

    static constexpr uint32_t queryIndex(Pass pass, bool end);
    void collectFrame(uint32_t slot);

    rhi::Device& m_device;
    std::array<rhi::TimestampQueryPoolHandle, BufferedFrames> m_query_pools{};
    std::array<uint64_t, QueryCount> m_timestamps{};
    diagnostics::GpuProfilerStats m_latest_stats{};
    uint64_t m_frame_index{0};
    uint32_t m_current_slot{0};
    bool m_supported{false};
};

} // namespace lunalite::renderer
