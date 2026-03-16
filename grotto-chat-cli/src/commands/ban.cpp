#include "../command.hpp"
#include "../command_context.hpp"

namespace grotto::cli::commands {

class BanCommand : public ICommand {
public:
    std::string name() const override { return "ban"; }
    std::string description() const override { return "Ban a user from the server"; }
    std::string usage() const override { return "grotto ban <user> [reason]"; }
    CommandType type() const override { return CommandType::fire_and_forget; }

    int execute(CommandContext& ctx) override {
        if (ctx.args.empty()) {
            ctx.error("Usage: " + usage());
            return 1;
        }

        std::string user_id = ctx.args[0];
        std::string reason = "Banned by admin";
        if (ctx.args.size() > 1) {
            reason.clear();
            for (size_t i = 1; i < ctx.args.size(); ++i) {
                if (i > 1) reason += " ";
                reason += ctx.args[i];
            }
        }

        auto resp = ctx.send_and_wait("ban", {
            {"user_id", user_id},
            {"reason", reason}
        });

        if (!resp) {
            ctx.error("No response from server (timeout)");
            return 1;
        }

        auto& r = *resp;
        if (r.value("ok", false)) {
            ctx.print("Banned " + user_id);
        } else {
            ctx.error("Error: " + r.value("error", "unknown"));
            return 1;
        }
        return 0;
    }
};

std::unique_ptr<ICommand> make_ban() {
    return std::make_unique<BanCommand>();
}

} // namespace grotto::cli::commands
