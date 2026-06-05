#include "../core/log.h"
#include "../script/script_runtime.h"
#include "components.h"
#include "scene.h"

#include <unordered_map>
#include <unordered_set>

namespace lunalite::scene {
namespace {
bool decomposeTransform(const glm::mat4& matrix, TransformComponent& transform)
{
    glm::vec3 translation{matrix[3]};
    glm::vec3 basis0{matrix[0]};
    glm::vec3 basis1{matrix[1]};
    glm::vec3 basis2{matrix[2]};

    glm::vec3 scale{
        glm::length(basis0),
        glm::length(basis1),
        glm::length(basis2),
    };

    constexpr float minScale = 0.000001f;
    if (scale.x <= minScale || scale.y <= minScale || scale.z <= minScale) {
        return false;
    }

    basis0 /= scale.x;
    basis1 /= scale.y;
    basis2 /= scale.z;

    if (glm::determinant(glm::mat3{basis0, basis1, basis2}) < 0.0f) {
        if (scale.x >= scale.y && scale.x >= scale.z) {
            scale.x = -scale.x;
            basis0 = -basis0;
        } else if (scale.y >= scale.z) {
            scale.y = -scale.y;
            basis1 = -basis1;
        } else {
            scale.z = -scale.z;
            basis2 = -basis2;
        }
    }

    transform.translation = translation;
    transform.rotation = glm::normalize(glm::quat_cast(glm::mat3{basis0, basis1, basis2}));
    transform.scale = scale;
    return true;
}

glm::mat4 getWorldTransformRecursive(const Scene& scene,
                                     Entity entity,
                                     std::unordered_set<entt::entity>& visited)
{
    if (!scene.isValidEntity(entity) || !visited.insert(entity.getHandle()).second) {
        return glm::mat4{1.0f};
    }

    glm::mat4 world{1.0f};
    if (scene.hasComponent<TransformComponent>(entity)) {
        world = scene.getComponent<TransformComponent>(entity).getTransform();
    }

    const auto parent = scene.getParent(entity);
    if (parent) {
        return getWorldTransformRecursive(scene, parent, visited) * world;
    }

    return world;
}

bool isParentOf(const Scene& scene, Entity possibleParent, Entity child)
{
    if (!possibleParent || !child) {
        return false;
    }

    std::unordered_set<entt::entity> visited;
    auto current = scene.getParent(child);
    while (current) {
        if (!visited.insert(current.getHandle()).second) {
            break;
        }
        if (current.getHandle() == possibleParent.getHandle()) {
            return true;
        }
        current = scene.getParent(current);
    }

    return false;
}

void destroyEntityRecursive(Scene& scene, Entity entity, std::unordered_set<entt::entity>& visited)
{
    if (!scene.isValidEntity(entity) || !visited.insert(entity.getHandle()).second) {
        return;
    }

    const auto children = scene.getChildren(entity);
    for (const auto child : children) {
        destroyEntityRecursive(scene, child, visited);
    }

    scene.getRegistry().destroy(entity.getHandle());
}
} // namespace

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
        std::unordered_set<entt::entity> visited;
        destroyEntityRecursive(*this, entity, visited);
    }
}

bool Scene::setParent(Entity child, Entity parent, bool keepWorldTransform)
{
    if (!isValidEntity(child) || (parent && !isValidEntity(parent)) || child.getHandle() == parent.getHandle() ||
        (parent && isParentOf(*this, child, parent))) {
        return false;
    }

    if (!hasComponent<TransformComponent>(child) || (parent && !hasComponent<TransformComponent>(parent))) {
        return false;
    }

    if (keepWorldTransform) {
        const auto childWorld = getWorldTransform(child);
        if (parent) {
            const auto parentWorld = getWorldTransform(parent);
            const auto parentInverse = glm::inverse(parentWorld);
            const auto local = parentInverse * childWorld;
            auto& transform = getComponent<TransformComponent>(child);
            if (!decomposeTransform(local, transform)) {
                return false;
            }
        } else {
            auto& transform = getComponent<TransformComponent>(child);
            if (!decomposeTransform(childWorld, transform)) {
                return false;
            }
        }
    }

    if (parent) {
        if (hasComponent<ParentComponent>(child)) {
            getComponent<ParentComponent>(child).parent = parent.getHandle();
        } else {
            addComponent<ParentComponent>(child).parent = parent.getHandle();
        }
    } else if (hasComponent<ParentComponent>(child)) {
        removeComponent<ParentComponent>(child);
    }

    return true;
}

bool Scene::clearParent(Entity child, bool keepWorldTransform)
{
    return setParent(child, Entity{}, keepWorldTransform);
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

    std::vector<std::pair<Entity, Entity>> entityMap;
    entityMap.reserve(other.getEntities().size());

    for (const auto sourceEntity : other.getEntities()) {
        auto targetEntity = createEntity();
        entityMap.emplace_back(sourceEntity, targetEntity);

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

    const auto findMappedEntity = [&entityMap](Entity sourceEntity) -> Entity {
        for (const auto& [mappedSource, mappedTarget] : entityMap) {
            if (mappedSource.getHandle() == sourceEntity.getHandle()) {
                return mappedTarget;
            }
        }
        return {};
    };

    for (const auto sourceEntity : other.getEntities()) {
        if (!other.hasComponent<ParentComponent>(sourceEntity)) {
            continue;
        }

        const auto parent = other.getComponent<ParentComponent>(sourceEntity).parent;
        if (parent == entt::null) {
            continue;
        }

        const auto targetEntity = findMappedEntity(sourceEntity);
        const auto mappedParent = findMappedEntity(Entity{parent});
        if (targetEntity && mappedParent) {
            addComponent<ParentComponent>(targetEntity).parent = mappedParent.getHandle();
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

std::vector<Entity> Scene::getRootEntities() const
{
    std::vector<Entity> roots;
    for (const auto entity : getEntities()) {
        if (!hasComponent<ParentComponent>(entity) || getComponent<ParentComponent>(entity).parent == entt::null ||
            !isValidEntity(Entity{getComponent<ParentComponent>(entity).parent})) {
            roots.push_back(entity);
        }
    }

    return roots;
}

std::vector<Entity> Scene::getChildren(Entity entity) const
{
    std::vector<Entity> children;
    if (!isValidEntity(entity)) {
        return children;
    }

    for (const auto candidate : getEntities()) {
        if (!hasComponent<ParentComponent>(candidate)) {
            continue;
        }

        if (getComponent<ParentComponent>(candidate).parent == entity.getHandle()) {
            children.push_back(candidate);
        }
    }

    return children;
}

Entity Scene::getParent(Entity entity) const
{
    if (!isValidEntity(entity) || !hasComponent<ParentComponent>(entity)) {
        return {};
    }

    const auto parent = getComponent<ParentComponent>(entity).parent;
    return parent != entt::null ? Entity{parent} : Entity{};
}

bool Scene::isAncestor(Entity possibleAncestor, Entity entity) const
{
    if (!possibleAncestor || !entity) {
        return false;
    }

    std::unordered_set<entt::entity> visited;
    auto current = getParent(entity);
    while (current) {
        if (!visited.insert(current.getHandle()).second) {
            break;
        }
        if (current.getHandle() == possibleAncestor.getHandle()) {
            return true;
        }
        current = getParent(current);
    }

    return false;
}

glm::mat4 Scene::getWorldTransform(Entity entity) const
{
    std::unordered_set<entt::entity> visited;
    return getWorldTransformRecursive(*this, entity, visited);
}

} // namespace lunalite::scene
