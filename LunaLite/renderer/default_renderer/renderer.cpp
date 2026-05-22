#include "renderer.h"

#include <cstddef>

#include <algorithm>
#include <vector>

namespace lunalite::renderer {
namespace {

constexpr const char* vertexShaderSource = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(std140, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad0;
    vec3 lightDir;
    float _pad1;
    vec3 lightAmbient;
    float _pad2;
    vec3 lightDiffuse;
    float _pad3;
    vec3 lightSpecular;
    float _pad4;
};

out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
    vWorldPos = aPosition;
    vNormal = aNormal;
    gl_Position = projection * view * vec4(aPosition, 1.0);
}
)";

constexpr const char* fragmentShaderSource = R"(
#version 450 core

in vec3 vWorldPos;
in vec3 vNormal;

layout(std140, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad0;
    vec3 lightDir;
    float _pad1;
    vec3 lightAmbient;
    float _pad2;
    vec3 lightDiffuse;
    float _pad3;
    vec3 lightSpecular;
    float _pad4;
};

out vec4 outColor;

const float shininess = 32.0;
const vec3 materialAmbient = vec3(0.3, 0.25, 0.2);
const vec3 materialDiffuse = vec3(0.8, 0.65, 0.5);
const vec3 materialSpecular = vec3(0.5, 0.5, 0.5);

void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-lightDir);
    vec3 V = normalize(cameraPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), shininess);

    vec3 ambient = lightAmbient * materialAmbient;
    vec3 diffuse = lightDiffuse * diff * materialDiffuse;
    vec3 specular = lightSpecular * spec * materialSpecular;

    vec3 color = ambient + diffuse + specular;
    outColor = vec4(color, 1.0);
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
                            .semantic = rhi::VertexAttribute::Normal,
                            .format = rhi::VertexFormat::Float3,
                            .offset = offsetof(interface::Vertex, normal),
                        },
                    },
            },
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
    });

    m_mesh_vertex_buffer_size = 0;
    m_mesh_vertex_buffer = m_device->createBuffer(
        rhi::BufferDesc{
            .type = rhi::BufferType::VertexBuffer,
            .usage = rhi::BufferUsage::Dynamic,
            .size = m_mesh_vertex_buffer_size,
        },
        nullptr);

    m_frameUniformBuffer = m_device->createBuffer(
        rhi::BufferDesc{
            .type = rhi::BufferType::UniformBuffer,
            .usage = rhi::BufferUsage::Dynamic,
            .size = sizeof(FrameUniforms),
        },
        nullptr);
}

void Renderer::beginFrame()
{
    m_cmd->beginFrame();
    m_cmd->clear(0.08f, 0.09f, 0.11f, 1.0f);
    m_cmd->bindPipeline(m_pipeline);

    m_device->updateBuffer(m_frameUniformBuffer, &m_frameUniforms, sizeof(FrameUniforms));
    m_cmd->bindUniformBuffer(m_frameUniformBuffer, 0);
}

void Renderer::endFrame()
{
    m_cmd->endFrame();
    m_cmd->present();
}

void Renderer::setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos)
{
    m_frameUniforms.view = view;
    m_frameUniforms.projection = proj;
    m_frameUniforms.cameraPos = cameraPos;
}

void Renderer::setDirectionalLight(const glm::vec3& direction,
                                   const glm::vec3& ambient,
                                   const glm::vec3& diffuse,
                                   const glm::vec3& specular)
{
    m_frameUniforms.lightDir = direction;
    m_frameUniforms.lightAmbient = ambient;
    m_frameUniforms.lightDiffuse = diffuse;
    m_frameUniforms.lightSpecular = specular;
}

void Renderer::renderMesh(const interface::Mesh& mesh, const glm::mat4& transform)
{
    std::vector<interface::Vertex> vertices;
    vertices.reserve(mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size());

    if (mesh.indices.empty()) {
        vertices = mesh.vertices;
    } else {
        for (const auto index : mesh.indices) {
            if (index < mesh.vertices.size()) {
                vertices.push_back(mesh.vertices[index]);
            }
        }
    }

    const auto normalMatrix = glm::mat3(transform);
    for (auto& vertex : vertices) {
        vertex.position = glm::vec3(transform * glm::vec4(vertex.position, 1.0f));
        vertex.normal = normalMatrix * vertex.normal;
    }

    const auto requiredSize = vertices.size() * sizeof(interface::Vertex);
    if (requiredSize > m_mesh_vertex_buffer_size) {
        m_device->destroyBuffer(m_mesh_vertex_buffer);
        m_mesh_vertex_buffer_size = requiredSize;
        m_mesh_vertex_buffer = m_device->createBuffer(
            rhi::BufferDesc{
                .type = rhi::BufferType::VertexBuffer,
                .usage = rhi::BufferUsage::Dynamic,
                .size = m_mesh_vertex_buffer_size,
            },
            nullptr);
    }

    m_device->updateBuffer(m_mesh_vertex_buffer, vertices.data(), requiredSize);
    m_cmd->bindVertexBuffer(m_mesh_vertex_buffer);
    m_cmd->draw(static_cast<uint32_t>(vertices.size()));
}

} // namespace lunalite::renderer
