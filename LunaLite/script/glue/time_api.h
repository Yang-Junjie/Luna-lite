#pragma once
#include "../../core/timestep.h"

namespace lunalite::script {
struct ScriptTime {
    float elapsed_time{0.0f};
    float delta_time{0.0f};
};

class TimeAPI {
public:
    static void reset(ScriptTime& time);
    static void update(ScriptTime& time, core::Timestep dt);
    static float getElapsedTime(const ScriptTime& time);
    static float getDeltaTime(const ScriptTime& time);
};
} // namespace lunalite::script
