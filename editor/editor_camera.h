#pragma once

#include "../LunaLite/core/timestep.h"
#include "../LunaLite/renderer/interface/camera.h"

#include <glm/glm.hpp>

namespace lunalite::editor {

class EditorCamera final : public renderer::interface::Camera {
public:
    using ProjectionType = renderer::interface::Camera::ProjectionType;

    EditorCamera();
    ~EditorCamera() override;

    void onUpdate(core::Timestep dt, bool viewportHovered);
    void setProjectionType(ProjectionType projection_type);

    void setPosition(const glm::vec3& position);
    glm::mat4 getView() const override;
    glm::vec3 getPosition() const override;

    void setYaw(float yaw);
    float getYaw() const;

    void setPitch(float pitch);
    float getPitch() const;

    void setMoveSpeed(float move_speed);
    float getMoveSpeed() const;

    void setMouseSensitivity(float mouse_sensitivity);
    float getMouseSensitivity() const;

private:
    void lockOrthographicState();
    bool isOrthographic() const;

    glm::vec3 getForwardDirection() const;
    glm::vec3 getRightDirection() const;

    glm::vec3 m_position{3.0f, 2.0f, 5.0f};
    float m_yaw{-120.0f};
    float m_pitch{-20.0f};
    float m_perspective_yaw{m_yaw};
    float m_perspective_pitch{m_pitch};
    float m_orthographic_locked_z{m_position.z};
    float m_move_speed{4.0f};
    float m_mouse_sensitivity{0.12f};
    bool m_is_controlling{false};
};

} // namespace lunalite::editor
