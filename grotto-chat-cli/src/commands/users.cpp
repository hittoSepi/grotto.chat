#include "../command.hpp"
#include "../command_context.hpp"
#include <iomanip>
#include <sstream>

namespace grotto::cli::commands {

class UsersCommand : public ICommand {
public:
    std::string name() const override { return "users"; }
    std::string description() const override { return "List online users"; }
    std::string usage() const override { return "grotto users"; }
    CommandType type() const override { return CommandType::fire_and_forget; }

    int execute(CommandContext& ctx) override {
        auto resp = ctx.send_and_wait("users");
        if (!resp) {
            ctx.error("No response from server (timeout)");
            return 1;
        }

        auto& r = *resp;
        if (!r.value("ok", false)) {
            ctx.error("Error: " + r.value("error", "unknown"));
            return 1;
        }

        auto users = r.value("data", nlohmann::json::array());
        if (users.empty()) {
            ctx.print("No users online.");
            return 0;
        }

        std::ostringstream out;
        out << std::left
            << std::setw(20) << "USER"
            << std::setw(18) << "IP"
            << "CONNECTED\n"
            << std::string(56, '-') << "\n";

        for (const auto& u : users) {
            out << std::setw(20) << u.value("id", "")
                << std::setw(18) << u.value("ip", "")
                << u.value("connected", "") << "\n";
        }

        ctx.print(out.str());
        return 0;
    }
};

std::unique_ptr<ICommand> make_users() {
    return std::make_unique<UsersCommand>();
}

} // namespace grotto::cli::commands
