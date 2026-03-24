#include "../command.hpp"
#include "../command_context.hpp"

namespace grotto::cli::commands {

class RestartCommand : public ICommand {
public:
    std::string name() const override { return "restart"; }
    std::string description() const override { return "Restart the server"; }
    std::string usage() const override { return "grotto restart"; }
    CommandType type() const override { return CommandType::fire_and_forget; }

    int execute(CommandContext& ctx) override {
        auto resp = ctx.send_and_wait("restart");
        if (!resp) {
            ctx.error("No response from server (timeout)");
            return 1;
        }

        auto& r = *resp;
        if (r.value("ok", false)) {
            ctx.print("Server restarting...");
        } else {
            ctx.error("Error: " + r.value("error", "unknown"));
            return 1;
        }
        return 0;
    }
};

std::unique_ptr<ICommand> make_restart() {
    return std::make_unique<RestartCommand>();
}

} // namespace grotto::cli::commands
