#pragma once
#include "../core/timestep.h"

#include <memory>

namespace lunalite::scene {
class Scene;
}

namespace lunalite::script {
enum class ScriptRuntimeBackend {
    Lua,
};

class ScriptRuntime {
public:
    virtual ~ScriptRuntime() = default;

    virtual void onRuntimeStart(scene::Scene& scene) = 0;
    virtual void onRuntimeUpdate(core::Timestep dt) = 0;
    virtual void onRuntimeStop() = 0;
};

std::unique_ptr<ScriptRuntime> createScriptRuntime(ScriptRuntimeBackend backend = ScriptRuntimeBackend::Lua);
} // namespace lunalite::script
