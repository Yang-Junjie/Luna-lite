#pragma once
#include "frame_image.h"
#include "mesh.h"
#include "model.h"
#include "render_lighting.h"

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
    virtual void
        setViewProjection(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, float exposure) = 0;
    virtual void setLighting(const RenderLighting& lighting) = 0;

    virtual void renderModel(const Model& model, const glm::mat4& transform) = 0;

    virtual void renderLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) = 0;

    virtual const FrameImage& getFrameImage() const = 0;
};
} // namespace lunalite::renderer::interface
