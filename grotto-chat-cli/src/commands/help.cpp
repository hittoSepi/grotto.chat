#include "../command.hpp"
#include "../command_context.hpp"
#include "../command_registry.hpp"
#include <sstream>
#include <iomanip>

namespace grotto::cli::commands {

class HelpCommand : public ICommand {
public:
    std::string name() const override { return "help"; }
    std::string description() const override { return "Show available commands"; }
    std::string usage() const override { return "grotto help [command]"; }
    CommandType type() const override { return CommandType::local; }

    int execute(CommandContext& ctx) override {
        if (!ctx.args.empty()) {
            auto* cmd = ctx.registry.find(ctx.args[0]);
            if (!cmd) {
                ctx.error("Unknown command: " + ctx.args[0]);
                return 1;
            }
            ctx.print(cmd->name() + " - " + cmd->description());
            ctx.print("Usage: " + cmd->usage());
            return 0;
        }

        ctx.print("grotto - Grotto server management CLI\n");
        ctx.print("Usage: grotto <command> [options]\n");
        ctx.print("Commands:");

        auto cmds = ctx.registry.all();
        size_t max_len = 0;
        for (auto* c : cmds) max_len = std::max(max_len, c->name().size());

        for (auto* c : cmds) {
            std::ostringstream line;
            line << "  " << std::left << std::setw(static_cast<int>(max_len + 2))
                 << c->name() << c->description();
            ctx.print(line.str());
        }

        ctx.print("\nRun 'grotto help <command>' for more info on a command.");
        return 0;
    }
};

std::unique_ptr<ICommand> make_help() {
    return std::make_unique<HelpCommand>();
}

} // namespace grotto::cli::commands
