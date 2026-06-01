#pragma once
#include "../interface/frame_image.h"
#include "../interface/renderer.h"

#include <cstdint>

#include <random>
#include <vector>

namespace lunalite::renderer {

class PathTracingRenderer final : public interface::Renderer {
public:
    PathTracingRenderer(uint32_t width, uint32_t height);

    void beginFrame() override;
    void endFrame() override;
    void resize(uint32_t width, uint32_t height) override;
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) override;
    void setSceneLighting(const interface::SceneLighting& lighting) override;
    void renderMesh(const interface::Mesh& mesh, const glm::mat4& transform) override;
    void renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) override;
    const interface::FrameImage& getFrameImage() const override;

private:
    struct Ray {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
    };

    struct Triangle {
        glm::vec3 p0{0.0f};
        glm::vec3 p1{0.0f};
        glm::vec3 p2{0.0f};
        glm::vec3 n0{0.0f, 1.0f, 0.0f};
        glm::vec3 n1{0.0f, 1.0f, 0.0f};
        glm::vec3 n2{0.0f, 1.0f, 0.0f};
        glm::vec3 c0{1.0f};
        glm::vec3 c1{1.0f};
        glm::vec3 c2{1.0f};
    };

    struct Hit {
        float t{0.0f};
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec3 albedo{1.0f};
    };

    void clearFrame();
    void renderFrame();
    void updateFrameImage();
    glm::vec3 tracePath(const Ray& initial_ray, std::mt19937& rng) const;
    bool intersectScene(const Ray& ray, Hit& hit) const;
    bool intersectTriangle(const Ray& ray, const Triangle& triangle, float& t, float& u, float& v) const;
    Ray generateCameraRay(float u, float v) const;
    glm::vec3 sampleCosineHemisphere(const glm::vec3& normal, std::mt19937& rng) const;
    glm::vec3 background(const glm::vec3& direction) const;
    size_t pixelIndex(uint32_t x, uint32_t y) const;

    uint32_t m_width{1};
    uint32_t m_height{1};
    uint32_t m_samples_per_pixel{4};
    uint32_t m_max_bounces{2};

    std::vector<glm::vec3> m_framebuffer;
    std::vector<glm::vec4> m_present_buffer;
    std::vector<Triangle> m_triangles;
    interface::FrameImage m_frame_image;

    glm::mat4 m_view{1.0f};
    glm::mat4 m_projection{1.0f};
    glm::mat4 m_inverse_view{1.0f};
    glm::mat4 m_inverse_projection{1.0f};
    glm::vec3 m_camera_pos{0.0f};
    interface::SceneLighting m_lighting;
};

} // namespace lunalite::renderer
