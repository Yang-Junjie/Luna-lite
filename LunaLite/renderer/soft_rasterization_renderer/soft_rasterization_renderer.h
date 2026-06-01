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
    void setSceneLighting(const interface::SceneLighting& lighting) override;
    void renderModel(const interface::Model& model, const glm::mat4& transform) override;
    void renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) override;
    const interface::FrameImage& getFrameImage() const override;

private:
    struct ScreenVertex {
        glm::vec3 position{0.0f};
        glm::vec3 world_position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec3 color{1.0f};
    };

    void rasterizeTriangle(const ScreenVertex& v0, const ScreenVertex& v1, const ScreenVertex& v2);
    void rasterizeLine(const ScreenVertex& v0, const ScreenVertex& v1);
    bool projectVertex(const interface::Vertex& vertex,
                       const glm::mat4& model,
                       const glm::mat4& normal_matrix,
                       ScreenVertex& out) const;
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
    interface::SceneLighting m_lighting;
};

} // namespace lunalite::renderer
