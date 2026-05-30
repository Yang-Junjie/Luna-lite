#pragma once
#include "../core/timestep.h"

namespace lunalite::scene {
class Scene;
}

namespace lunalite::script {
class ScriptRuntime {
public:
    virtual ~ScriptRuntime() = default;

    virtual void onRuntimeStart(scene::Scene& scene) = 0;
    virtual void onRuntimeUpdate(core::Timestep dt) = 0;
    virtual void onRuntimeStop() = 0;
};
} // namespace lunalite::script
