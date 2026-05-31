#include "../../core/input.h"
#include "input_api.h"

#include <cctype>

#include <algorithm>
#include <unordered_map>

namespace lunalite::script {
namespace {
std::string normalizeName(std::string value)
{
    std::erase_if(value, [](char c) {
        return c == '_' || c == '-' || c == ' ';
    });

    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

core::KeyCode keyCodeFromString(const std::string& key)
{
    static const std::unordered_map<std::string, core::KeyCode> keyCodes{
        {"LEFTSHIFT", core::KeyCode::LeftShift},
        {"RIGHTSHIFT", core::KeyCode::RightShift},
        {"LEFTCONTROL", core::KeyCode::LeftControl},
        {"RIGHTCONTROL", core::KeyCode::RightControl},
        {"LEFTCTRL", core::KeyCode::LeftControl},
        {"RIGHTCTRL", core::KeyCode::RightControl},
        {"LEFTALT", core::KeyCode::LeftAlt},
        {"RIGHTALT", core::KeyCode::RightAlt},
        {"SPACE", core::KeyCode::Space},
        {"ENTER", core::KeyCode::Enter},
        {"DELETE", core::KeyCode::Delete},
        {"ESCAPE", core::KeyCode::Escape},
        {"ESC", core::KeyCode::Escape},
        {"UP", core::KeyCode::Up},
        {"DOWN", core::KeyCode::Down},
        {"LEFT", core::KeyCode::Left},
        {"RIGHT", core::KeyCode::Right},
        {"BACKSPACE", core::KeyCode::Backspace},
    };

    const auto normalized = normalizeName(key);
    if (normalized.size() == 1 && normalized[0] >= 'A' && normalized[0] <= 'Z') {
        return static_cast<core::KeyCode>(normalized[0]);
    }

    if (const auto it = keyCodes.find(normalized); it != keyCodes.end()) {
        return it->second;
    }

    return core::KeyCode::None;
}

core::MouseCode mouseCodeFromString(const std::string& button)
{
    static const std::unordered_map<std::string, core::MouseCode> mouseCodes{
        {"LEFT", core::MouseCode::Left},
        {"RIGHT", core::MouseCode::Right},
        {"MIDDLE", core::MouseCode::Middle},
        {"XBUTTON1", core::MouseCode::XButton1},
        {"XBUTTON2", core::MouseCode::XButton2},
    };

    const auto normalized = normalizeName(button);
    if (const auto it = mouseCodes.find(normalized); it != mouseCodes.end()) {
        return it->second;
    }

    return core::MouseCode::None;
}
} // namespace

bool InputAPI::isKeyPressed(const std::string& key)
{
    return core::Input::isKeyPressed(keyCodeFromString(key));
}

bool InputAPI::isMouseButtonPressed(const std::string& button)
{
    return core::Input::isMouseButtonPressed(mouseCodeFromString(button));
}

glm::vec2 InputAPI::getMousePosition()
{
    return core::Input::getMousePosition();
}

glm::vec2 InputAPI::getMouseDelta()
{
    return core::Input::getMouseDelta();
}

glm::vec2 InputAPI::getMouseScroll()
{
    return core::Input::getMouseScrollOffset();
}

} // namespace lunalite::script
