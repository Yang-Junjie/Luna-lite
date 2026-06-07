#include "gpu_profiler.h"
#include "TinyRHI/interface/command_list.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/timestamp_query.h"

#include <cstddef>

#include <algorithm>

namespace lunalite::renderer {
namespace {

float elapsedMilliseconds(uint64_t begin_ns, uint64_t end_ns)
{
    if (end_ns < begin_ns) {
        return 0.0f;
    }

    return static_cast<float>(end_ns - begin_ns) / 1'000'000.0f;
}

} // namespace

GpuProfiler::GpuProfiler(rhi::Device& device)
    : m_device(device)
{
    for (auto& pool : m_query_pools) {
        pool = m_device.createTimestampQueryPool(rhi::TimestampQueryPoolDesc{.count = QueryCount});
        if (!pool) {
            for (auto existingPool : m_query_pools) {
                if (existingPool) {
                    m_device.destroyTimestampQueryPool(existingPool);
                }
            }
            m_query_pools = {};
            return;
        }
    }

    m_supported = true;
    m_latest_stats.supported = true;
}

GpuProfiler::~GpuProfiler()
{
    for (auto pool : m_query_pools) {
        if (pool) {
            m_device.destroyTimestampQueryPool(pool);
        }
    }
}

const diagnostics::GpuProfilerStats& GpuProfiler::beginFrame(rhi::CommandList& cmd)
{
    if (!m_supported) {
        m_latest_stats = diagnostics::GpuProfilerStats{};
        return m_latest_stats;
    }

    m_current_slot = static_cast<uint32_t>(m_frame_index % BufferedFrames);
    collectFrame(m_current_slot);

    const auto pool = m_query_pools[m_current_slot];
    cmd.resetTimestampQueries(pool, 0, QueryCount);
    cmd.writeTimestamp(pool, queryIndex(Pass::Frame, false));
    m_frame_index += 1;
    return m_latest_stats;
}

void GpuProfiler::beginPass(rhi::CommandList& cmd, Pass pass)
{
    if (!m_supported) {
        return;
    }

    cmd.writeTimestamp(m_query_pools[m_current_slot], queryIndex(pass, false));
}

void GpuProfiler::endPass(rhi::CommandList& cmd, Pass pass)
{
    if (!m_supported) {
        return;
    }

    cmd.writeTimestamp(m_query_pools[m_current_slot], queryIndex(pass, true));
}

void GpuProfiler::recordEmptyPass(rhi::CommandList& cmd, Pass pass)
{
    beginPass(cmd, pass);
    endPass(cmd, pass);
}

void GpuProfiler::endFrame(rhi::CommandList& cmd)
{
    if (!m_supported) {
        return;
    }

    cmd.writeTimestamp(m_query_pools[m_current_slot], queryIndex(Pass::Frame, true));
}

GpuProfiler::Pass GpuProfiler::shadowCascadePass(uint32_t cascade_index)
{
    switch (cascade_index) {
        case 0:
            return Pass::ShadowCascade0;
        case 1:
            return Pass::ShadowCascade1;
        case 2:
            return Pass::ShadowCascade2;
        default:
            return Pass::ShadowCascade3;
    }
}

constexpr uint32_t GpuProfiler::queryIndex(Pass pass, bool end)
{
    return static_cast<uint32_t>(pass) * QueriesPerPass + (end ? 1u : 0u);
}

void GpuProfiler::collectFrame(uint32_t slot)
{
    diagnostics::GpuProfilerStats stats;
    stats.supported = true;

    if (!m_device.getTimestampQueryResults(m_query_pools[slot], 0, QueryCount, m_timestamps.data())) {
        m_latest_stats = stats;
        return;
    }

    stats.valid = true;
    stats.frame_ms =
        elapsedMilliseconds(m_timestamps[queryIndex(Pass::Frame, false)], m_timestamps[queryIndex(Pass::Frame, true)]);
    stats.shadow_ms = elapsedMilliseconds(m_timestamps[queryIndex(Pass::Shadow, false)],
                                          m_timestamps[queryIndex(Pass::Shadow, true)]);
    for (uint32_t cascadeIndex = 0; cascadeIndex < diagnostics::MaxShadowCascadeStats; ++cascadeIndex) {
        const auto pass = shadowCascadePass(cascadeIndex);
        stats.shadow_cascade_ms[cascadeIndex] =
            elapsedMilliseconds(m_timestamps[queryIndex(pass, false)], m_timestamps[queryIndex(pass, true)]);
    }
    stats.geometry_ms = elapsedMilliseconds(m_timestamps[queryIndex(Pass::Geometry, false)],
                                            m_timestamps[queryIndex(Pass::Geometry, true)]);
    stats.lighting_ms = elapsedMilliseconds(m_timestamps[queryIndex(Pass::Lighting, false)],
                                            m_timestamps[queryIndex(Pass::Lighting, true)]);
    stats.skybox_ms = elapsedMilliseconds(m_timestamps[queryIndex(Pass::Skybox, false)],
                                          m_timestamps[queryIndex(Pass::Skybox, true)]);
    stats.sprites_ms = elapsedMilliseconds(m_timestamps[queryIndex(Pass::Sprites, false)],
                                           m_timestamps[queryIndex(Pass::Sprites, true)]);
    stats.debug_lines_ms = elapsedMilliseconds(m_timestamps[queryIndex(Pass::DebugLines, false)],
                                               m_timestamps[queryIndex(Pass::DebugLines, true)]);

    m_latest_stats = stats;
}

} // namespace lunalite::renderer
