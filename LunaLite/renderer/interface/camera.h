#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace lunalite::renderer::interface {

class Camera {
public:
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    virtual ~Camera() = default;

    void setPerspective(float verticalFov, float nearClip, float farClip)
    {
        m_projection_type = ProjectionType::Perspective;
        m_perspective_fov = verticalFov;
        m_perspective_near = nearClip;
        m_perspective_far = farClip;
    }

    void setOrthographic(float size, float nearClip, float farClip)
    {
        m_projection_type = ProjectionType::Orthographic;
        m_orthographic_size = size;
        m_orthographic_near = nearClip;
        m_orthographic_far = farClip;
    }

    ProjectionType getProjectionType() const
    {
        return m_projection_type;
    }

    void setExposure(float exposure)
    {
        m_exposure = exposure < 0.0f ? 0.0f : exposure;
    }

    float getExposure() const
    {
        return m_exposure;
    }

    virtual glm::mat4 getProjection(float aspectRatio) const
    {
        if (m_projection_type == ProjectionType::Orthographic) {
            const float halfHeight = m_orthographic_size * 0.5f;
            const float halfWidth = halfHeight * aspectRatio;
            return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, m_orthographic_near, m_orthographic_far);
        }

        return glm::perspective(m_perspective_fov, aspectRatio, m_perspective_near, m_perspective_far);
    }

    virtual glm::mat4 getView() const
    {
        return glm::mat4{1.0f};
    }

    virtual glm::vec3 getPosition() const
    {
        return glm::vec3{0.0f};
    }

private:
    ProjectionType m_projection_type{ProjectionType::Perspective};

    float m_perspective_fov{glm::radians(45.0f)};
    float m_perspective_near{0.1f};
    float m_perspective_far{100.0f};

    float m_orthographic_size{10.0f};
    float m_orthographic_near{0.1f};
    float m_orthographic_far{100.0f};

    float m_exposure{1.0f};
};

} // namespace lunalite::renderer::interface
