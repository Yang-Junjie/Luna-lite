#pragma once
#include "frame_image.h"
#include "mesh.h"

#include <cstdint>

#include <glm/glm.hpp>

namespace lunalite::renderer::interface {
class Renderer {
public:
    Renderer() = default;
    virtual ~Renderer() = default;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual void setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos) = 0;
    virtual void setDirectionalLight(const glm::vec3& direction,
                                     const glm::vec3& ambient,
                                     const glm::vec3& diffuse,
                                     const glm::vec3& specular) = 0;
    virtual void renderMesh(const Mesh& mesh, const glm::mat4& transform) = 0;
    virtual const FrameImage& getFrameImage() const = 0;
};
} // namespace lunalite::renderer::interface
