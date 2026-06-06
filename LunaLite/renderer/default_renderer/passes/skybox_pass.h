#pragma once
#include "../renderer_common.h"
#include "TinyRHI/interface/command_list.h"

namespace lunalite::renderer {

class SkyboxPass {
public:
    SkyboxPass(rhi::CommandList& commands,
               rhi::PipelineHandle skybox_pipeline,
               rhi::BindGroupHandle geometry_bind_group);

    void execute(const GBuffer& gbuffer, rhi::BindGroupHandle environment_bind_group);

private:
    rhi::CommandList* m_cmd{nullptr};
    rhi::PipelineHandle m_skybox_pipeline{};
    rhi::BindGroupHandle m_geometry_bind_group{};
};

} // namespace lunalite::renderer
