#include "../core/log.h"
#include "../script/script_runtime.h"
#include "components.h"
#include "scene.h"

namespace lunalite::scene {

Entity Scene::createEntity()
{
    Entity entity{m_registry.create()};
    addComponent<TagComponent>(entity);
    addComponent<TransformComponent>(entity);
    return entity;
}

void Scene::destroyEntity(Entity entity)
{
    if (isValidEntity(entity)) {
        m_registry.destroy(entity.getHandle());
    }
}

void Scene::clear()
{
    if (m_script_runtime) {
        LUNA_CORE_DEBUG("Stopping scene runtime before clearing scene");
        m_script_runtime->onRuntimeStop();
        m_script_runtime.reset();
    }
    m_registry.clear();
    m_settings = {};
}

void Scene::copyFrom(const Scene& other)
{
    clear();
    m_settings = other.m_settings;

    for (const auto sourceEntity : other.getEntities()) {
        auto targetEntity = createEntity();

        if (other.hasComponent<TagComponent>(sourceEntity)) {
            getComponent<TagComponent>(targetEntity) = other.getComponent<TagComponent>(sourceEntity);
        }

        if (other.hasComponent<TransformComponent>(sourceEntity)) {
            getComponent<TransformComponent>(targetEntity) = other.getComponent<TransformComponent>(sourceEntity);
        }

        if (other.hasComponent<ModelComponent>(sourceEntity)) {
            addComponent<ModelComponent>(targetEntity) = other.getComponent<ModelComponent>(sourceEntity);
        }

        if (other.hasComponent<ScriptComponent>(sourceEntity)) {
            addComponent<ScriptComponent>(targetEntity) = other.getComponent<ScriptComponent>(sourceEntity);
        }

        if (other.hasComponent<CameraComponent>(sourceEntity)) {
            addComponent<CameraComponent>(targetEntity) = other.getComponent<CameraComponent>(sourceEntity);
        }

        if (other.hasComponent<DirectionalLightComponent>(sourceEntity)) {
            addComponent<DirectionalLightComponent>(targetEntity) =
                other.getComponent<DirectionalLightComponent>(sourceEntity);
        }
    }
}

void Scene::onRuntimeStart()
{
    if (m_script_runtime) {
        LUNA_CORE_WARN("Scene runtime was already running; restarting it");
        m_script_runtime->onRuntimeStop();
    }

    m_script_runtime = script::createScriptRuntime();
    LUNA_ASSERT(m_script_runtime, "Failed to create script runtime.");
    LUNA_CORE_INFO("Scene runtime started");
    m_script_runtime->onRuntimeStart(*this);
}

void Scene::onUpdateEditor(core::Timestep dt)
{
    (void) dt;
}

void Scene::onUpdateRuntime(core::Timestep dt)
{
    if (m_script_runtime) {
        m_script_runtime->onRuntimeUpdate(dt);
    }
}

void Scene::onRuntimeStop()
{
    if (m_script_runtime) {
        m_script_runtime->onRuntimeStop();
        m_script_runtime.reset();
        LUNA_CORE_INFO("Scene runtime stopped");
    }
}

bool Scene::isValidEntity(Entity entity) const
{
    return m_registry.valid(entity.getHandle());
}

std::vector<Entity> Scene::getEntities() const
{
    std::vector<Entity> entities;
    const auto* storage = m_registry.storage<entt::entity>();
    entities.reserve(storage->size());

    for (const auto [handle] : storage->each()) {
        entities.emplace_back(handle);
    }

    return entities;
}

} // namespace lunalite::scene
