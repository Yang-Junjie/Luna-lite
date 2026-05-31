#pragma once

#include <glm/glm.hpp>
#include <string>

namespace lunalite::script {
class InputAPI {
public:
    static bool isKeyPressed(const std::string& key);
    static bool isMouseButtonPressed(const std::string& button);
    static glm::vec2 getMousePosition();
    static glm::vec2 getMouseDelta();
    static glm::vec2 getMouseScroll();
};
} // namespace lunalite::script
