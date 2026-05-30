#pragma once
#include "../scene/entity.h"
#include "script_runtime.h"

#include <filesystem>
#include <sol/sol.hpp>
#include <vector>

namespace lunalite::script {
struct LuaScriptEntity {
    scene::Scene* scene{};
    scene::Entity entity{};
};

struct LuaScriptInstance {
    LuaScriptEntity entity;
    std::filesystem::path script_path;
    sol::table table;
    sol::protected_function on_create;
    sol::protected_function on_update;
    sol::protected_function on_destroy;
};

class LuaScriptRuntime final : public ScriptRuntime {
public:
    LuaScriptRuntime();

    void onRuntimeStart(scene::Scene& scene) override;
    void onRuntimeUpdate(core::Timestep dt) override;
    void onRuntimeStop() override;

private:
    void registerBindings();
    void reportScriptError(const sol::protected_function_result& result,
                           const std::filesystem::path& scriptPath,
                           const char* functionName) const;

    sol::state m_lua;
    std::vector<LuaScriptInstance> m_instances;
};
} // namespace lunalite::script
