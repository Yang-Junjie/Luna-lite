#include "time_api.h"

namespace lunalite::script {

void TimeAPI::reset(ScriptTime& time)
{
    time.elapsed_time = 0.0f;
    time.delta_time = 0.0f;
}

void TimeAPI::update(ScriptTime& time, core::Timestep dt)
{
    time.delta_time = dt.getSeconds();
    time.elapsed_time += time.delta_time;
}

float TimeAPI::getElapsedTime(const ScriptTime& time)
{
    return time.elapsed_time;
}

float TimeAPI::getDeltaTime(const ScriptTime& time)
{
    return time.delta_time;
}

} // namespace lunalite::script
