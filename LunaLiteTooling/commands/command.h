#pragma once

#include "../context/tool_context.h"
#include "command_args.h"
#include "command_result.h"

#include <functional>
#include <string>

namespace lunalite::tooling {

using CommandHandler = std::function<CommandResult(ToolContext&, const CommandArgs&)>;

struct CommandDesc {
    std::string id;
    std::string label;
    std::string category;
    CommandHandler execute;
};

} // namespace lunalite::tooling
