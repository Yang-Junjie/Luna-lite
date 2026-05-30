#include "../asset/asset_manager.h"
#include "../core/log.h"
#include "../project/project_manager.h"
#include "../scene/components.h"
#include "../scene/scene.h"
#include "entity_api.h"
#include "lua_script_runtime.h"

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
} // namespace

LuaScriptRuntime::LuaScriptRuntime()
{
    m_lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::string);
    registerBindings();
}

void LuaScriptRuntime::onRuntimeStart(scene::Scene& scene)
{
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    if (!projectRoot) {
        LUNA_CORE_ERROR("Failed to start Lua scripts: no project is loaded");
        return;
    }

    const auto view = scene.getRegistry().view<scene::ScriptComponent>();
    for (const auto entity : view) {
        const auto& scriptComponent = view.get<scene::ScriptComponent>(entity);
        for (const auto& binding : scriptComponent.scripts) {
            if (!binding.enabled || !binding.script.isValid()) {
                continue;
            }

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
}

void LuaScriptRuntime::onRuntimeUpdate(core::Timestep dt)
{
    for (auto& instance : m_instances) {
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
    for (auto& instance : m_instances) {
        if (!instance.on_destroy.valid()) {
            continue;
        }

        const auto result = instance.on_destroy(instance.entity);
        if (!result.valid()) {
            reportScriptError(result, instance.script_path, "on_destroy");
        }
    }

    m_instances.clear();
}

void LuaScriptRuntime::registerBindings()
{
    m_lua.new_usertype<LuaScriptEntity>(
        "Entity",
        "get_tag",
        [](LuaScriptEntity& entity) {
            return EntityAPI::getTag(*entity.scene, entity.entity);
        },
        "set_tag",
        [](LuaScriptEntity& entity, const std::string& tag) {
            EntityAPI::setTag(*entity.scene, entity.entity, tag);
        },
        "get_translation",
        [](LuaScriptEntity& entity) {
            return toTuple(EntityAPI::getTranslation(*entity.scene, entity.entity));
        },
        "set_translation",
        [](LuaScriptEntity& entity, float x, float y, float z) {
            EntityAPI::setTranslation(*entity.scene, entity.entity, toVec3(x, y, z));
        },
        "get_rotation",
        [](LuaScriptEntity& entity) {
            return toTuple(EntityAPI::getRotation(*entity.scene, entity.entity));
        },
        "set_rotation",
        [](LuaScriptEntity& entity, float x, float y, float z) {
            EntityAPI::setRotation(*entity.scene, entity.entity, toVec3(x, y, z));
        },
        "get_scale",
        [](LuaScriptEntity& entity) {
            return toTuple(EntityAPI::getScale(*entity.scene, entity.entity));
        },
        "set_scale",
        [](LuaScriptEntity& entity, float x, float y, float z) {
            EntityAPI::setScale(*entity.scene, entity.entity, toVec3(x, y, z));
        });
}

void LuaScriptRuntime::reportScriptError(const sol::protected_function_result& result,
                                         const std::filesystem::path& scriptPath,
                                         const char* functionName) const
{
    sol::error error = result;
    LUNA_CORE_ERROR("Lua script '{}' {} failed: {}", scriptPath.string(), functionName, error.what());
}

} // namespace lunalite::script
