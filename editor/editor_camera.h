#pragma once

#include "../LunaLite/core/timestep.h"
#include "../LunaLite/renderer/interface/camera.h"

#include <glm/glm.hpp>

namespace lunalite::editor {

class EditorCamera final : public renderer::interface::Camera {
public:
    EditorCamera();
    ~EditorCamera() override;

    void onUpdate(core::Timestep dt, bool viewportHovered);

    glm::mat4 getView() const override;
    glm::vec3 getPosition() const override;

private:
    glm::vec3 getForwardDirection() const;
    glm::vec3 getRightDirection() const;

    glm::vec3 m_position{3.0f, 2.0f, 5.0f};
    float m_yaw{-120.0f};
    float m_pitch{-20.0f};
    float m_move_speed{4.0f};
    float m_mouse_sensitivity{0.12f};
    bool m_is_controlling{false};
};

} // namespace lunalite::editor
