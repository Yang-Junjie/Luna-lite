#include "skybox_pass.h"

namespace lunalite::renderer {

SkyboxPass::SkyboxPass(rhi::CommandList& commands,
                       rhi::PipelineHandle skybox_pipeline,
                       rhi::BindGroupHandle geometry_bind_group)
    : m_cmd(&commands),
      m_skybox_pipeline(skybox_pipeline),
      m_geometry_bind_group(geometry_bind_group)
{}

bool SkyboxPass::execute(const GBuffer& gbuffer, rhi::BindGroupHandle environment_bind_group)
{
    const rhi::TextureTransition depthAttachmentBarrier{
        .texture = gbuffer.depth_texture,
        .state = rhi::ResourceState::DepthStencilWrite,
    };
    m_cmd->transition(&depthAttachmentBarrier, 1);

    rhi::RenderPassBeginInfo skyboxPass;
    skyboxPass.color_attachments.push_back(rhi::ColorAttachmentDesc{
        .view = gbuffer.final_color_view,
        .load_op = rhi::LoadOp::Load,
        .store_op = rhi::StoreOp::Store,
    });
    skyboxPass.has_depth_stencil_attachment = true;
    skyboxPass.depth_stencil_attachment = rhi::DepthStencilAttachmentDesc{
        .view = gbuffer.depth_view,
        .depth_load_op = rhi::LoadOp::Load,
        .depth_store_op = rhi::StoreOp::DontCare,
    };
    skyboxPass.width = gbuffer.width;
    skyboxPass.height = gbuffer.height;

    m_cmd->beginRenderPass(skyboxPass);
    m_cmd->setPipeline(m_skybox_pipeline);
    m_cmd->setBindGroup(0, m_geometry_bind_group);
    m_cmd->setBindGroup(1, environment_bind_group);
    m_cmd->draw(3);
    m_cmd->endRenderPass();
    return true;
}

} // namespace lunalite::renderer
