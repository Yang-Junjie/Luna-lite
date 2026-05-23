#pragma once
#include "../../rhi/interface/instance.h"
#include "../interface/renderer.h"

#include <glm/glm.hpp>

namespace lunalite::renderer {
class Renderer : public interface::Renderer {
public:
    explicit Renderer(rhi::Instance& rhi);
    ~Renderer() override = default;
    void beginFrame() override;
    void endFrame() override;
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) override;
    void setDirectionalLight(const glm::vec3& direction,
                             const glm::vec3& ambient,
                             const glm::vec3& diffuse,
                             const glm::vec3& specular) override;
    void renderMesh(const interface::Mesh& mesh, const glm::mat4& transform) override;

    struct alignas(16) FrameUniforms {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 cameraPos;
        float _pad0;
        glm::vec3 lightDir;
        float _pad1;
        glm::vec3 lightAmbient;
        float _pad2;
        glm::vec3 lightDiffuse;
        float _pad3;
        glm::vec3 lightSpecular;
        float _pad4;
    };

private:
    rhi::Device* m_device{nullptr};
    rhi::CommandContext* m_cmd{nullptr};
    rhi::PipelineHandle m_pipeline{0};
    rhi::BufferHandle m_mesh_vertex_buffer{0};
    size_t m_mesh_vertex_buffer_size{0};
    rhi::BufferHandle m_frameUniformBuffer{0};
    FrameUniforms m_frameUniforms;
};
} // namespace lunalite::renderer
