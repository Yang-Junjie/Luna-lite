#pragma once
#include "frame_image.h"
#include "mesh.h"

#include <cstdint>

#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
struct DirectionalLightData {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 ambient{0.0f};
    glm::vec3 diffuse{0.0f};
    glm::vec3 specular{0.0f};
};

struct SceneLighting {
    uint32_t directional_light_count{0};
    DirectionalLightData directional_light{};
    glm::vec3 environment_ambient{0.0f};
};

class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) = 0;
    virtual void setSceneLighting(const SceneLighting& lighting) = 0;

    virtual void renderMesh(const Mesh& mesh, const glm::mat4& transform) = 0;

    virtual const FrameImage& getFrameImage() const = 0;
};
} // namespace lunalite::renderer::interface
