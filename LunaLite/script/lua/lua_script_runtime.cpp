#include "../../asset/asset_manager.h"
#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "../../scene/components.h"
#include "../../scene/scene.h"
#include "../glue/entity_api.h"
#include "../glue/input_api.h"
#include "../glue/log_api.h"
#include "../glue/scene_api.h"
#include "../glue/time_api.h"
#include "lua_script_runtime.h"

#include <cstdint>

#include <charconv>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>

namespace lunalite::script {
namespace {
std::tuple<float, float, float> toTuple(const glm::vec3& value)
{
    return {value.x, value.y, value.z};
}

glm::vec3 toVec3(float x, float y, float z)
{
    return {x, y, z};
}

std::optional<asset::AssetHandle> assetHandleFromLua(const sol::object& value)
{
    if (value.is<uint64_t>()) {
        return asset::AssetHandle{value.as<uint64_t>()};
    }

    if (!value.is<std::string>()) {
        return std::nullopt;
    }

    const auto text = value.as<std::string>();
    uint64_t handle = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, handle);
    if (error != std::errc{} || ptr != end) {
        return std::nullopt;
    }

    return asset::AssetHandle{handle};
}
} // namespace

LuaScriptRuntime::LuaScriptRuntime()
{
    m_lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::string);
    registerBindings();
}

void LuaScriptRuntime::onRuntimeStart(scene::Scene& scene)
{
    TimeAPI::reset(m_time);
    updateTime(core::Timestep{0.0f});
    registerSceneBindings(scene);

    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot) {
        LUNA_CORE_ERROR("Failed to start Lua scripts: no project is loaded");
        return;
    }

    const auto view = scene.getRegistry().view<scene::ScriptComponent>();
    size_t enabledScriptCount = 0;
    for (const auto entity : view) {
        const auto& scriptComponent = view.get<scene::ScriptComponent>(entity);
        for (const auto& binding : scriptComponent.scripts) {
            if (!binding.enabled || !binding.script.isValid()) {
                continue;
            }

            ++enabledScriptCount;
            const auto* metadata = asset::AssetManager::get().getMetadata(binding.script);
            if (metadata == nullptr || metadata->Type != asset::AssetType::Script) {
                LUNA_CORE_ERROR("Failed to load Lua script: invalid script asset {}", binding.script.toString());
                continue;
            }

            const auto scriptPath = *projectRoot / metadata->FilePath;
            sol::load_result loadResult = m_lua.load_file(scriptPath.string());
            if (!loadResult.valid()) {
                sol::error error = loadResult;
                LUNA_CORE_ERROR("Failed to load Lua script '{}': {}", scriptPath.string(), error.what());
                continue;
            }

            sol::protected_function_result result = loadResult();
            if (!result.valid()) {
                reportScriptError(result, scriptPath, "load");
                continue;
            }

            sol::object scriptObject = result.get<sol::object>(0);
            if (!scriptObject.is<sol::table>()) {
                LUNA_CORE_ERROR("Failed to load Lua script '{}': script must return a table", scriptPath.string());
                continue;
            }

            LuaScriptInstance instance;
            instance.entity = LuaScriptEntity{&scene, scene::Entity{entity}};
            instance.script_path = scriptPath;
            instance.table = scriptObject.as<sol::table>();
            instance.on_create = instance.table["on_create"];
            instance.on_update = instance.table["on_update"];
            instance.on_destroy = instance.table["on_destroy"];

            if (instance.on_create.valid()) {
                const auto createResult = instance.on_create(instance.entity);
                if (!createResult.valid()) {
                    reportScriptError(createResult, instance.script_path, "on_create");
                    continue;
                }
            }

            m_instances.push_back(std::move(instance));
        }
    }

    LUNA_CORE_INFO("Lua runtime started with {} script instance(s) from {} enabled binding(s)",
                   m_instances.size(),
                   enabledScriptCount);
}

void LuaScriptRuntime::onRuntimeUpdate(core::Timestep dt)
{
    updateTime(dt);

    for (auto& instance : m_instances) {
        if (instance.entity.scene == nullptr || !instance.entity.scene->isValidEntity(instance.entity.entity)) {
            continue;
        }

        if (!instance.on_update.valid()) {
            continue;
        }

        const auto result = instance.on_update(instance.entity, dt.getSeconds());
        if (!result.valid()) {
            reportScriptError(result, instance.script_path, "on_update");
        }
    }
}

void LuaScriptRuntime::onRuntimeStop()
{
    const auto instanceCount = m_instances.size();
    for (auto& instance : m_instances) {
        if (instance.entity.scene == nullptr || !instance.entity.scene->isValidEntity(instance.entity.entity)) {
            continue;
        }

        if (!instance.on_destroy.valid()) {
            continue;
        }

        const auto result = instance.on_destroy(instance.entity);
        if (!result.valid()) {
            reportScriptError(result, instance.script_path, "on_destroy");
        }
    }

    m_instances.clear();
    LUNA_CORE_INFO("Lua runtime stopped after {} script instance(s)", instanceCount);
}

void LuaScriptRuntime::registerBindings()
{
    m_lua["Time"] = m_lua.create_table_with("elapsed_time", 0.0f, "delta_time", 0.0f);

    m_lua["Log"] = m_lua.create_table_with(
        "info",
        [](const std::string& message) {
            LogAPI::info(message);
        },
        "warn",
        [](const std::string& message) {
            LogAPI::warn(message);
        },
        "error",
        [](const std::string& message) {
            LogAPI::error(message);
        });

    m_lua["Input"] = m_lua.create_table_with(
        "is_key_pressed",
        [](const std::string& key) {
            return InputAPI::isKeyPressed(key);
        },
        "is_mouse_button_pressed",
        [](const std::string& button) {
            return InputAPI::isMouseButtonPressed(button);
        },
        "get_mouse_position",
        []() {
            const auto position = InputAPI::getMousePosition();
            return std::tuple<float, float>{position.x, position.y};
        },
        "get_mouse_delta",
        []() {
            const auto delta = InputAPI::getMouseDelta();
            return std::tuple<float, float>{delta.x, delta.y};
        },
        "get_mouse_scroll",
        []() {
            const auto scroll = InputAPI::getMouseScroll();
            return std::tuple<float, float>{scroll.x, scroll.y};
        });

    m_lua.new_usertype<LuaScriptEntity>(
        "Entity",
        "get_handle",
        [](LuaScriptEntity& entity) {
            return EntityAPI::getHandle(entity.entity);
        },
        "is_valid",
        [](LuaScriptEntity& entity) {
            return entity.scene != nullptr && EntityAPI::isValid(*entity.scene, entity.entity);
        },
        "destroy",
        [](LuaScriptEntity& entity) {
            if (entity.scene != nullptr) {
                EntityAPI::destroy(*entity.scene, entity.entity);
            }
        },
        "get_tag",
        [](LuaScriptEntity& entity) {
            if (entity.scene == nullptr) {
                return std::string{};
            }
            return EntityAPI::getTag(*entity.scene, entity.entity);
        },
        "set_tag",
        [](LuaScriptEntity& entity, const std::string& tag) {
            if (entity.scene != nullptr) {
                EntityAPI::setTag(*entity.scene, entity.entity, tag);
            }
        },
        "get_translation",
        [](LuaScriptEntity& entity) {
            if (entity.scene == nullptr) {
                return toTuple(glm::vec3{0.0f});
            }
            return toTuple(EntityAPI::getTranslation(*entity.scene, entity.entity));
        },
        "set_translation",
        [](LuaScriptEntity& entity, float x, float y, float z) {
            if (entity.scene != nullptr) {
                EntityAPI::setTranslation(*entity.scene, entity.entity, toVec3(x, y, z));
            }
        },
        "get_rotation",
        [](LuaScriptEntity& entity) {
            if (entity.scene == nullptr) {
                return toTuple(glm::vec3{0.0f});
            }
            return toTuple(EntityAPI::getRotation(*entity.scene, entity.entity));
        },
        "set_rotation",
        [](LuaScriptEntity& entity, float x, float y, float z) {
            if (entity.scene != nullptr) {
                EntityAPI::setRotation(*entity.scene, entity.entity, toVec3(x, y, z));
            }
        },
        "set_rotation_euler",
        [](LuaScriptEntity& entity, float x, float y, float z) {
            if (entity.scene != nullptr) {
                EntityAPI::setRotationEuler(*entity.scene, entity.entity, toVec3(x, y, z));
            }
        },
        "set_orientation_quat",
        [](LuaScriptEntity& entity, float x, float y, float z, float w) {
            if (entity.scene != nullptr) {
                EntityAPI::setOrientationQuat(*entity.scene, entity.entity, glm::quat{w, x, y, z});
            }
        },
        "set_yaw_pitch",
        [](LuaScriptEntity& entity, float yaw, float pitch) {
            if (entity.scene != nullptr) {
                EntityAPI::setYawPitch(*entity.scene, entity.entity, yaw, pitch);
            }
        },
        "get_scale",
        [](LuaScriptEntity& entity) {
            if (entity.scene == nullptr) {
                return toTuple(glm::vec3{1.0f});
            }
            return toTuple(EntityAPI::getScale(*entity.scene, entity.entity));
        },
        "set_scale",
        [](LuaScriptEntity& entity, float x, float y, float z) {
            if (entity.scene != nullptr) {
                EntityAPI::setScale(*entity.scene, entity.entity, toVec3(x, y, z));
            }
        },
        "has_mesh_renderer",
        [](LuaScriptEntity& entity) {
            return entity.scene != nullptr && EntityAPI::hasMeshRenderer(*entity.scene, entity.entity);
        },
        "get_mesh_renderer",
        [](LuaScriptEntity& entity) {
            if (entity.scene == nullptr) {
                return uint64_t{0};
            }
            return static_cast<uint64_t>(EntityAPI::getMeshRenderer(*entity.scene, entity.entity));
        },
        "set_mesh_renderer",
        [](LuaScriptEntity& entity, const sol::object& handle) {
            if (entity.scene == nullptr) {
                return;
            }

            const auto mesh = assetHandleFromLua(handle);
            if (!mesh) {
                LogAPI::error("Entity.set_mesh_renderer expects an integer handle or decimal handle string");
                return;
            }

            EntityAPI::setMeshRenderer(*entity.scene, entity.entity, *mesh);
        },
        "remove_mesh_renderer",
        [](LuaScriptEntity& entity) {
            if (entity.scene != nullptr) {
                EntityAPI::removeMeshRenderer(*entity.scene, entity.entity);
            }
        },
        "has_camera",
        [](LuaScriptEntity& entity) {
            return entity.scene != nullptr && EntityAPI::hasCamera(*entity.scene, entity.entity);
        },
        "add_camera",
        [](LuaScriptEntity& entity) {
            if (entity.scene != nullptr) {
                EntityAPI::addCamera(*entity.scene, entity.entity);
            }
        },
        "remove_camera",
        [](LuaScriptEntity& entity) {
            if (entity.scene != nullptr) {
                EntityAPI::removeCamera(*entity.scene, entity.entity);
            }
        },
        "has_directional_light",
        [](LuaScriptEntity& entity) {
            return entity.scene != nullptr && EntityAPI::hasDirectionalLight(*entity.scene, entity.entity);
        },
        "add_directional_light",
        [](LuaScriptEntity& entity) {
            if (entity.scene != nullptr) {
                EntityAPI::addDirectionalLight(*entity.scene, entity.entity);
            }
        },
        "remove_directional_light",
        [](LuaScriptEntity& entity) {
            if (entity.scene != nullptr) {
                EntityAPI::removeDirectionalLight(*entity.scene, entity.entity);
            }
        });
}

void LuaScriptRuntime::registerSceneBindings(scene::Scene& scene)
{
    m_lua["Scene"] = m_lua.create_table_with(
        "create_entity",
        [&scene](sol::optional<std::string> tag) {
            auto entity = SceneAPI::createEntity(scene, tag ? *tag : std::string{});
            return LuaScriptEntity{&scene, entity};
        },
        "destroy_entity",
        [&scene](LuaScriptEntity& entity) {
            SceneAPI::destroyEntity(scene, entity.entity);
        },
        "find_entity_by_tag",
        [&scene](const std::string& tag) {
            return LuaScriptEntity{&scene, SceneAPI::findEntityByTag(scene, tag)};
        });
}

void LuaScriptRuntime::updateTime(core::Timestep dt)
{
    TimeAPI::update(m_time, dt);

    sol::table time = m_lua["Time"];
    time["delta_time"] = TimeAPI::getDeltaTime(m_time);
    time["elapsed_time"] = TimeAPI::getElapsedTime(m_time);
}

void LuaScriptRuntime::reportScriptError(const sol::protected_function_result& result,
                                         const std::filesystem::path& scriptPath,
                                         const char* functionName) const
{
    sol::error error = result;
    LUNA_CORE_ERROR("Lua script '{}' {} failed: {}", scriptPath.string(), functionName, error.what());
}

} // namespace lunalite::script
