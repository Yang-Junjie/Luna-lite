#include "../LunaLite/renderer/interface/sprite_geometry.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace {
using lunalite::renderer::interface::SpriteDrawCommand;

bool visible(const SpriteDrawCommand& sprite)
{
    return lunalite::renderer::interface::spriteIntersectsClipVolume(sprite, glm::mat4{1.0f});
}
} // namespace

int main()
{
    SpriteDrawCommand sprite;
    sprite.size = {1.0f, 1.0f};
    sprite.pivot = {0.5f, 0.5f};

    if (!visible(sprite)) {
        std::cerr << "Sprite inside clip space should be visible.\n";
        return 1;
    }

    sprite.transform = glm::translate(glm::mat4{1.0f}, glm::vec3{2.0f, 0.0f, 0.0f});
    if (visible(sprite)) {
        std::cerr << "Sprite fully outside the right clip plane should be culled.\n";
        return 1;
    }

    sprite.transform = glm::translate(glm::mat4{1.0f}, glm::vec3{1.2f, 0.0f, 0.0f});
    if (!visible(sprite)) {
        std::cerr << "Sprite crossing the right clip plane should remain visible.\n";
        return 1;
    }

    sprite.size = {0.5f, 0.5f};
    sprite.pivot = {1.0f, 0.5f};
    sprite.transform = glm::translate(glm::mat4{1.0f}, glm::vec3{1.2f, 0.0f, 0.0f});
    if (!visible(sprite)) {
        std::cerr << "Sprite pivot should affect clip testing.\n";
        return 1;
    }

    sprite.size = {1.0f, 1.0f};
    sprite.pivot = {0.5f, 0.5f};
    sprite.transform = glm::translate(glm::mat4{1.0f}, glm::vec3{0.0f, 0.0f, 2.0f});
    if (visible(sprite)) {
        std::cerr << "Sprite fully outside the far clip plane should be culled.\n";
        return 1;
    }

    std::cout << "Sprite clip-volume culling keeps visible sprites and culls off-camera sprites.\n";
    return 0;
}
