#include "editor_camera.h"

#include "../LunaLite/core/input.h"
#include "../LunaLite/core/key_codes.h"
#include "../LunaLite/core/mouse_codes.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace lunalite::editor {

EditorCamera::EditorCamera()
{
    setPerspective(glm::radians(45.0f), 0.1f, 100.0f);
}

EditorCamera::~EditorCamera()
{
    if (m_is_controlling) {
        core::Input::setRawMouseMotion(false);
        core::Input::setCursorMode(core::CursorMode::Normal);
    }
}

void EditorCamera::onUpdate(core::Timestep dt, bool viewportHovered)
{
    const bool rightMouseDown = core::Input::isMouseButtonPressed(core::MouseCode::Right);
    const bool shouldStartControl = rightMouseDown && viewportHovered;

    if (!m_is_controlling && shouldStartControl) {
        m_is_controlling = true;
        core::Input::setRawMouseMotion(true);
        core::Input::setCursorMode(core::CursorMode::Locked);
    } else if (m_is_controlling && !rightMouseDown) {
        m_is_controlling = false;
        core::Input::setRawMouseMotion(false);
        core::Input::setCursorMode(core::CursorMode::Normal);
    }

    if (!m_is_controlling) {
        return;
    }

    const glm::vec2 mouseDelta = core::Input::getMouseDelta();
    m_yaw += mouseDelta.x * m_mouse_sensitivity;
    m_pitch -= mouseDelta.y * m_mouse_sensitivity;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    float speed = m_move_speed;
    if (core::Input::isKeyPressed(core::KeyCode::LeftShift) || core::Input::isKeyPressed(core::KeyCode::RightShift)) {
        speed *= 3.0f;
    }

    const float distance = speed * dt.getSeconds();
    const glm::vec3 forward = getForwardDirection();
    const glm::vec3 right = getRightDirection();
    const glm::vec3 up{0.0f, 1.0f, 0.0f};

    if (core::Input::isKeyPressed(core::KeyCode::W)) {
        m_position += forward * distance;
    }
    if (core::Input::isKeyPressed(core::KeyCode::S)) {
        m_position -= forward * distance;
    }
    if (core::Input::isKeyPressed(core::KeyCode::D)) {
        m_position += right * distance;
    }
    if (core::Input::isKeyPressed(core::KeyCode::A)) {
        m_position -= right * distance;
    }
    if (core::Input::isKeyPressed(core::KeyCode::E)) {
        m_position += up * distance;
    }
    if (core::Input::isKeyPressed(core::KeyCode::Q)) {
        m_position -= up * distance;
    }
}

glm::mat4 EditorCamera::getView() const
{
    return glm::lookAt(m_position, m_position + getForwardDirection(), glm::vec3{0.0f, 1.0f, 0.0f});
}

glm::vec3 EditorCamera::getPosition() const
{
    return m_position;
}

glm::vec3 EditorCamera::getForwardDirection() const
{
    const float yaw = glm::radians(m_yaw);
    const float pitch = glm::radians(m_pitch);

    return glm::normalize(glm::vec3{
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw),
    });
}

glm::vec3 EditorCamera::getRightDirection() const
{
    return glm::normalize(glm::cross(getForwardDirection(), glm::vec3{0.0f, 1.0f, 0.0f}));
}

} // namespace lunalite::editor
