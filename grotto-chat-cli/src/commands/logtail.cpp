#include "../command.hpp"
#include "../command_context.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

namespace grotto::cli::commands {

namespace {
    std::atomic<bool> g_running{true};

    void signal_handler(int) {
        g_running = false;
    }
}

class LogtailCommand : public ICommand {
public:
    std::string name() const override { return "logtail"; }
    std::string description() const override { return "Stream server logs in real-time"; }
    std::string usage() const override { return "grotto logtail [--level <level>]"; }
    CommandType type() const override { return CommandType::streaming; }

    int execute(CommandContext& ctx) override {
        std::string filter_level = ctx.flags.count("level") ? ctx.flags["level"] : "";

        g_running = true;
        auto prev_handler = std::signal(SIGINT, signal_handler);

        ctx.subscribe([&filter_level](const nlohmann::json& event) {
            if (event.value("type", "") != "log") return;

            auto data = event.value("data", nlohmann::json::object());
            std::string level = data.value("level", "");
            if (!filter_level.empty() && level != filter_level) return;

            std::string ts = data.value("ts", "");
            std::string msg = data.value("msg", "");

            // Color by level
            std::string prefix;
            if (level == "error")      prefix = "\033[31m[ERR]\033[0m";
            else if (level == "warn")  prefix = "\033[33m[WRN]\033[0m";
            else if (level == "info")  prefix = "\033[32m[INF]\033[0m";
            else if (level == "debug") prefix = "\033[36m[DBG]\033[0m";
            else                       prefix = "[" + level + "]";

            std::cout << ts << " " << prefix << " " << msg << std::endl;
        });

        // Send subscribe command to ensure we get log events
        ctx.send_and_wait("subscribe", {{"events", {"log"}}}, 2000);

        ctx.print("Streaming logs... (Ctrl+C to stop)\n");

        while (g_running && ctx.connected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::signal(SIGINT, prev_handler);
        return 0;
    }
};

std::unique_ptr<ICommand> make_logtail() {
    return std::make_unique<LogtailCommand>();
}

} // namespace grotto::cli::commands
