#include "../command.hpp"
#include "../command_context.hpp"

namespace grotto::cli::commands {

class KickCommand : public ICommand {
public:
    std::string name() const override { return "kick"; }
    std::string description() const override { return "Kick a user from the server"; }
    std::string usage() const override { return "grotto kick <user> [reason]"; }
    CommandType type() const override { return CommandType::fire_and_forget; }

    int execute(CommandContext& ctx) override {
        if (ctx.args.empty()) {
            ctx.error("Usage: " + usage());
            return 1;
        }

        std::string user_id = ctx.args[0];
        std::string reason = "Kicked by admin";
        if (ctx.args.size() > 1) {
            reason.clear();
            for (size_t i = 1; i < ctx.args.size(); ++i) {
                if (i > 1) reason += " ";
                reason += ctx.args[i];
            }
        }

        auto resp = ctx.send_and_wait("kick", {
            {"user_id", user_id},
            {"reason", reason}
        });

        if (!resp) {
            ctx.error("No response from server (timeout)");
            return 1;
        }

        auto& r = *resp;
        if (r.value("ok", false)) {
            ctx.print("Kicked " + user_id);
        } else {
            ctx.error("Error: " + r.value("error", "unknown"));
            return 1;
        }
        return 0;
    }
};

std::unique_ptr<ICommand> make_kick() {
    return std::make_unique<KickCommand>();
}

} // namespace grotto::cli::commands
