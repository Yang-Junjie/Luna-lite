#pragma once
#include "../interface/frame_image.h"
#include "../interface/frame_render_data.h"
#include "../interface/render_lighting.h"
#include "../interface/renderer.h"
#include "renderer_common.h"
#include "TinyRHI/interface/device.h"
#include "TinyRHI/interface/swapchain.h"

#include <cstdint>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace lunalite::renderer {

class DebugLinePass;
class EnvironmentMapCache;
class GBufferResource;
class GpuProfiler;
class GeometryPass;
class LightingPass;
class MaterialGpuCache;
class RendererPipelineResources;
class ShadowMapResource;
class ShadowPass;
class SkyboxPass;
class TextureGpuCache;

class Renderer : public interface::Renderer {
public:
    Renderer(rhi::Device& device, rhi::Swapchain& swapchain);
    ~Renderer() override;
    void beginFrame() override;
    void endFrame() override;
    void resize(uint32_t width, uint32_t height) override;
    void renderFrame(const interface::FrameRenderData& frame) override;
    const interface::FrameImage& getFrameImage() const override;
    const diagnostics::RenderStats& getStats() const override;

private:
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, float exposure);
    void setLighting(const interface::RenderLighting& lighting);

    rhi::Device* m_device{nullptr};
    rhi::Swapchain* m_swapchain{nullptr};
    rhi::CommandListHandle m_command_list{};
    rhi::CommandList* m_cmd{nullptr};
    rhi::CommandListHandle m_upload_command_list{};
    rhi::CommandListHandle m_compute_command_list{};
    rhi::CommandList* m_compute_cmd{nullptr};

    std::unique_ptr<RendererPipelineResources> m_pipeline_resources;
    std::unique_ptr<GBufferResource> m_gbuffer;
    std::unique_ptr<TextureGpuCache> m_texture_gpu_cache;
    std::unique_ptr<MaterialGpuCache> m_material_gpu_cache;
    std::unique_ptr<EnvironmentMapCache> m_environment_map_cache;
    std::unique_ptr<ShadowMapResource> m_shadow_map;
    std::unique_ptr<GpuProfiler> m_gpu_profiler;
    std::unique_ptr<GeometryPass> m_geometry_pass;
    std::unique_ptr<ShadowPass> m_shadow_pass;
    std::unique_ptr<DebugLinePass> m_debug_line_pass;
    std::unique_ptr<LightingPass> m_lighting_pass;
    std::unique_ptr<SkyboxPass> m_skybox_pass;
    FrameUniforms m_frameUniforms;
    diagnostics::RenderStats m_stats;
    std::vector<interface::LineDrawCommand> m_pending_debug_lines;
    bool m_frame_uniforms_dirty{true};
    bool m_geometry_pass_recorded_this_frame{false};
};
} // namespace lunalite::renderer
