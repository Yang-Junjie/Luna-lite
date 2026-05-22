#pragma once
#include <entt/entt.hpp>

namespace lunalite::scene {
class Entity {
public:
    Entity() = default;
    explicit Entity(entt::entity handle)
        : m_handle(handle)
    {}
    Entity(const Entity&) = default;

    entt::entity getHandle() const
    {
        return m_handle;
    }

    explicit operator bool() const
    {
        return m_handle != entt::null;
    }

private:
    entt::entity m_handle{entt::null};
};
} // namespace lunalite::scene
