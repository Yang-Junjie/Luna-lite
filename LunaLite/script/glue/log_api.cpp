#include "../../core/log.h"
#include "log_api.h"

namespace lunalite::script {

void LogAPI::info(const std::string& message)
{
    LUNA_CORE_INFO("[Lua] {}", message);
}

void LogAPI::warn(const std::string& message)
{
    LUNA_CORE_WARN("[Lua] {}", message);
}

void LogAPI::error(const std::string& message)
{
    LUNA_CORE_ERROR("[Lua] {}", message);
}

} // namespace lunalite::script
