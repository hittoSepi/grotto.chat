#pragma once

#include <string>
#include <vector>

namespace grotto::cli {

class CommandContext; // forward

enum class CommandType {
    fire_and_forget,  // send command, get response, exit
    streaming,        // subscribe to events, print until Ctrl+C
    interactive,      // launch TUI or interactive mode
    local,            // no socket connection needed
};

/// Base interface for all CLI commands (builtin and script).
class ICommand {
public:
    virtual ~ICommand() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::string usage() const = 0;
    virtual CommandType type() const = 0;

    /// Execute the command. Returns exit code (0 = success).
    virtual int execute(CommandContext& ctx) = 0;
};

} // namespace grotto::cli
