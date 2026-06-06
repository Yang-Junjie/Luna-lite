#include "lighting_pass.h"

namespace lunalite::renderer {

LightingPass::LightingPass(rhi::CommandList& commands, rhi::PipelineHandle lighting_pipeline)
    : m_cmd(&commands),
      m_lighting_pipeline(lighting_pipeline)
{}

void LightingPass::execute(const GBuffer& gbuffer,
                           rhi::BindGroupHandle environment_bind_group,
                           rhi::BindGroupHandle shadow_lighting_bind_group,
                           rhi::TextureHandle shadow_map)
{
    const rhi::TextureTransition barriers[] = {
        rhi::TextureTransition{
            .texture = gbuffer.albedo_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = gbuffer.normal_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = gbuffer.material_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = gbuffer.emission_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
        rhi::TextureTransition{
            .texture = gbuffer.depth_texture,
            .state = rhi::ResourceState::ShaderRead,
        },
    };
    m_cmd->transition(barriers, 5);
    if (shadow_map) {
        const rhi::TextureTransition shadowMapBarrier{
            .texture = shadow_map,
            .state = rhi::ResourceState::ShaderRead,
        };
        m_cmd->transition(&shadowMapBarrier, 1);
    }

    rhi::RenderPassBeginInfo lightingPass;
    lightingPass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = gbuffer.final_color_view,
        .load_op = rhi::LoadOp::Clear,
        .store_op = rhi::StoreOp::Store,
        .clear_color = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
    });
    lightingPass.width = gbuffer.width;
    lightingPass.height = gbuffer.height;

    m_cmd->beginRenderPass(lightingPass);
    m_cmd->setPipeline(m_lighting_pipeline);
    m_cmd->setBindGroup(0, gbuffer.lighting_bind_group);
    m_cmd->setBindGroup(1, environment_bind_group);
    m_cmd->setBindGroup(2, shadow_lighting_bind_group);
    m_cmd->draw(3);
    m_cmd->endRenderPass();
}

void LightingPass::transitionFinalColorForSampling(const GBuffer& gbuffer)
{
    const rhi::TextureTransition finalColorBarrier{
        .texture = gbuffer.final_color_texture,
        .state = rhi::ResourceState::ShaderRead,
    };
    m_cmd->transition(&finalColorBarrier, 1);
}

} // namespace lunalite::renderer
