#include "renderer.h"

#include <cstddef>

namespace lunalite::renderer {
namespace {

constexpr const char* vertexShaderSource = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 3) in vec3 aColor;

out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = vec4(aPosition, 1.0);
}
)";

constexpr const char* fragmentShaderSource = R"(
#version 450 core

in vec3 vColor;
out vec4 outColor;

void main()
{
    outColor = vec4(vColor, 1.0);
}
)";

} // namespace

Renderer::Renderer(rhi::Instance& rhi)
    : m_device(rhi.getDevice())
{
    m_cmd = &m_device->getImmediateCmdContext();

    const auto vertexShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Vertex,
        .source = vertexShaderSource,
    });

    const auto fragmentShader = m_device->createShader(rhi::ShaderDesc{
        .stage = rhi::ShaderStage::Fragment,
        .source = fragmentShaderSource,
    });

    m_pipeline = m_device->createPipeline(rhi::PipelineDesc{
        .topology = rhi::PrimitiveTopology::Triangle,
        .vertex_layout =
            rhi::VertexLayoutDesc{
                .stride = sizeof(interface::Vertex),
                .attributes =
                    {
                        rhi::VertexAttributeDesc{
                            .semantic = rhi::VertexAttribute::Position,
                            .format = rhi::VertexFormat::Float3,
                            .offset = offsetof(interface::Vertex, position),
                        },
                        rhi::VertexAttributeDesc{
                            .semantic = rhi::VertexAttribute::Color,
                            .format = rhi::VertexFormat::Float3,
                            .offset = offsetof(interface::Vertex, color),
                        },
                    },
            },
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
    });
}

void Renderer::beginFrame()
{
    m_cmd->beginFrame();
    m_cmd->clear(0.08f, 0.09f, 0.11f, 1.0f);
    m_cmd->bindPipeline(m_pipeline);
    m_frame_triangle_index = 0;
}

void Renderer::endFrame()
{
    m_cmd->endFrame();
    m_cmd->present();
}

void Renderer::renderTriangle(const interface::Triangle& triangle)
{
    const interface::Vertex vertices[] = {
        triangle.getV1(),
        triangle.getV2(),
        triangle.getV3(),
    };

    if (m_frame_triangle_index >= m_triangle_buffers.size()) {
        const auto vertexBuffer = m_device->createBuffer(
            rhi::BufferDesc{
                .type = rhi::BufferType::VertexBuffer,
                .usage = rhi::BufferUsage::Static,
                .size = sizeof(vertices),
            },
            vertices);

        m_triangle_buffers.push_back(vertexBuffer);
    }

    const auto vertexBuffer = m_triangle_buffers[m_frame_triangle_index];
    ++m_frame_triangle_index;

    m_cmd->bindVertexBuffer(vertexBuffer);
    m_cmd->draw(3);
}

} // namespace lunalite::renderer
