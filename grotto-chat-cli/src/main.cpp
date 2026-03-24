#include "command.hpp"
#include "command_registry.hpp"
#include "command_context.hpp"
#include "commands/commands.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace grotto::cli;

static void register_builtins(CommandRegistry& reg) {
    reg.add(commands::make_help());
    reg.add(commands::make_version());
    reg.add(commands::make_status());
    reg.add(commands::make_users());
    reg.add(commands::make_shutdown());
    reg.add(commands::make_restart());
    reg.add(commands::make_kick());
    reg.add(commands::make_ban());
    reg.add(commands::make_logtail());
    reg.add(commands::make_tui());
    reg.add(commands::make_update());
}

static void parse_args(int argc, char* argv[],
                       std::string& command_name,
                       std::vector<std::string>& args,
                       std::map<std::string, std::string>& flags)
{
    // Skip argv[0] (program name)
    int i = 1;

    // First non-flag argument is the command name
    if (i < argc && argv[i][0] != '-') {
        command_name = argv[i];
        ++i;
    }

    // Parse remaining as args and flags
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.starts_with("--")) {
            std::string key = arg.substr(2);
            // Check if next arg is a value (not another flag)
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                flags[key] = argv[i + 1];
                ++i;
            } else {
                flags[key] = "true";
            }
        } else if (arg.starts_with("-") && arg.size() == 2) {
            // Short flag: -l <value>
            std::string key(1, arg[1]);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                flags[key] = argv[i + 1];
                ++i;
            } else {
                flags[key] = "true";
            }
        } else {
            args.push_back(arg);
        }
    }
}

int main(int argc, char* argv[]) {
    CommandRegistry registry;
    register_builtins(registry);

    // TODO: scan ~/.grotto/commands/ for script commands

    std::string command_name;
    std::vector<std::string> args;
    std::map<std::string, std::string> flags;

    parse_args(argc, argv, command_name, args, flags);

    // No command = show help
    if (command_name.empty()) {
        command_name = "help";
    }

    auto* cmd = registry.find(command_name);
    if (!cmd) {
        std::cerr << "Unknown command: " << command_name << "\n"
                  << "Run 'grotto help' for available commands.\n";
        return 1;
    }

    CommandContext ctx(registry);
    ctx.args = std::move(args);
    ctx.flags = std::move(flags);

    // Commands that need a socket connection
    if (cmd->type() != CommandType::local) {
        std::string socket_path = ctx.flags.count("socket")
            ? ctx.flags["socket"] : "";

        // TUI handles its own connection
        if (cmd->type() != CommandType::interactive) {
            if (!ctx.connect(socket_path)) {
                std::cerr << "Could not connect to server.\n"
                          << "Is grotto-server running?\n";
                return 2;
            }
        }
    }

    return cmd->execute(ctx);
}
