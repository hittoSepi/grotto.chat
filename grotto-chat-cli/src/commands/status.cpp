#include "../command.hpp"
#include "../command_context.hpp"
#include <iomanip>
#include <sstream>

namespace grotto::cli::commands {

class StatusCommand : public ICommand {
public:
    std::string name() const override { return "status"; }
    std::string description() const override { return "Show server status"; }
    std::string usage() const override { return "grotto status"; }
    CommandType type() const override { return CommandType::fire_and_forget; }

    int execute(CommandContext& ctx) override {
        auto resp = ctx.send_and_wait("status");
        if (!resp) {
            ctx.error("No response from server (timeout)");
            return 1;
        }

        auto& r = *resp;
        if (!r.value("ok", false)) {
            ctx.error("Error: " + r.value("error", "unknown"));
            return 1;
        }

        auto data = r.value("data", nlohmann::json::object());
        int64_t uptime = data.value("uptime", int64_t(0));
        int conns = data.value("connections", 0);
        std::string ver = data.value("version", "unknown");

        int hours = static_cast<int>(uptime / 3600);
        int mins = static_cast<int>((uptime % 3600) / 60);
        int secs = static_cast<int>(uptime % 60);

        std::ostringstream out;
        out << "Server Status\n"
            << "  Version:     " << ver << "\n"
            << "  Uptime:      " << hours << "h " << mins << "m " << secs << "s\n"
            << "  Connections: " << conns;
        ctx.print(out.str());
        return 0;
    }
};

std::unique_ptr<ICommand> make_status() {
    return std::make_unique<StatusCommand>();
}

} // namespace grotto::cli::commands
