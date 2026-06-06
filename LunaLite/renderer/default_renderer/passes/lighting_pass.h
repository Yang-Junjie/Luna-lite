#pragma once
#include "../renderer_common.h"
#include "TinyRHI/interface/command_list.h"

namespace lunalite::renderer {

class LightingPass {
public:
    LightingPass(rhi::CommandList& commands, rhi::PipelineHandle lighting_pipeline);

    void execute(const GBuffer& gbuffer,
                 rhi::BindGroupHandle environment_bind_group,
                 rhi::BindGroupHandle shadow_lighting_bind_group,
                 rhi::TextureHandle shadow_map);
    void transitionFinalColorForSampling(const GBuffer& gbuffer);

private:
    rhi::CommandList* m_cmd{nullptr};
    rhi::PipelineHandle m_lighting_pipeline{};
};

} // namespace lunalite::renderer
