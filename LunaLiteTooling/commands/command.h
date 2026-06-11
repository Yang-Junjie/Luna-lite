#pragma once

#include "../context/tool_context.h"
#include "command_args.h"
#include "command_result.h"

#include <string_view>

namespace lunalite::tooling {

class Command {
public:
    virtual ~Command() = default;

    virtual std::string_view id() const = 0;
    virtual std::string_view label() const = 0;
    virtual std::string_view category() const = 0;

    virtual CommandResult execute(ToolContext& context, const CommandArgs& args) = 0;

    virtual bool canUndo() const
    {
        return false;
    }

    virtual CommandResult undo(ToolContext&, const CommandArgs&)
    {
        return CommandResult::fail("Undo not supported");
    }
};

} // namespace lunalite::tooling
