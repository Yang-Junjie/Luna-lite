#include "lua/lua_script_runtime.h"
#include "script_runtime.h"

namespace lunalite::script {

std::unique_ptr<ScriptRuntime> createScriptRuntime(ScriptRuntimeBackend backend)
{
    switch (backend) {
        case ScriptRuntimeBackend::Lua:
        default:
            return std::make_unique<LuaScriptRuntime>();
    }
}

} // namespace lunalite::script
