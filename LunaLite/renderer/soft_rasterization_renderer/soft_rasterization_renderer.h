#pragma once
#include "../interface/frame_image.h"
#include "../interface/renderer.h"

#include <cstdint>
#include <vector>

namespace lunalite::renderer {

class SoftRasterizationRenderer final : public interface::Renderer {
public:
    SoftRasterizationRenderer(uint32_t width, uint32_t height);

    void beginFrame() override;
    void endFrame() override;
    void resize(uint32_t width, uint32_t height) override;
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) override;
    void setDirectionalLight(const glm::vec3& direction,
                             const glm::vec3& ambient,
                             const glm::vec3& diffuse,
                             const glm::vec3& specular) override;
    void renderMesh(const interface::Mesh& mesh, const glm::mat4& transform) override;
    const interface::FrameImage& getFrameImage() const override;

private:
    struct ScreenVertex {
        glm::vec3 position{0.0f};
        glm::vec3 world_position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec3 color{1.0f};
    };

    void rasterizeTriangle(const ScreenVertex& v0, const ScreenVertex& v1, const ScreenVertex& v2);
    bool projectVertex(const interface::Vertex& vertex, const glm::mat4& model, const glm::mat4& normal_matrix, ScreenVertex& out) const;
    void updateFrameImage();
    uint32_t pixelIndex(uint32_t x, uint32_t y) const;

    uint32_t m_width{0};
    uint32_t m_height{0};

    std::vector<glm::vec3> m_color_buffer;
    std::vector<uint8_t> m_present_buffer;
    std::vector<float> m_depth_buffer;
    interface::FrameImage m_frame_image;

    glm::mat4 m_view{1.0f};
    glm::mat4 m_projection{1.0f};
    glm::vec3 m_camera_pos{0.0f};
    glm::vec3 m_light_direction{0.0f, -1.0f, 0.0f};
    glm::vec3 m_light_ambient{0.05f};
    glm::vec3 m_light_diffuse{0.8f};
    glm::vec3 m_light_specular{1.0f};
};

} // namespace lunalite::renderer
