#include "../LunaLite/core/application.h"
#include "../LunaLite/renderer/debug_renderer.h"
#include "test_app_layer.h"

#include <glm/glm.hpp>

namespace lunalite::test_app {

TestAppLayer::TestAppLayer()
    : Layer("TestAppLayer")
{}

void TestAppLayer::onRender()
{
    auto& debugRenderer = core::Application::get().getDebugRenderer();
    debugRenderer.setViewProjection(glm::mat4{1.0f}, glm::mat4{1.0f}, glm::vec3{0.0f}, 1.0f);
    debugRenderer.renderLine(glm::vec3{-0.8f, -0.8f, 0.0f}, glm::vec3{0.8f, 0.8f, 0.0f}, glm::vec3{1.0f, 0.0f, 0.0f});
    debugRenderer.renderLine(glm::vec3{-0.8f, 0.8f, 0.0f}, glm::vec3{0.8f, -0.8f, 0.0f}, glm::vec3{0.0f, 0.8f, 1.0f});
}
} // namespace lunalite::test_app
