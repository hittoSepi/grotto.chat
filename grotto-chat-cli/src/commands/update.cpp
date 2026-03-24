#include "../command.hpp"
#include "../command_context.hpp"

namespace grotto::cli::commands {

class UpdateCommand : public ICommand {
public:
    std::string name() const override { return "update"; }
    std::string description() const override { return "Update and restart the server"; }
    std::string usage() const override { return "grotto update"; }
    CommandType type() const override { return CommandType::fire_and_forget; }

    int execute(CommandContext& ctx) override {
        auto resp = ctx.send_and_wait("update", {}, 30000); // longer timeout for update
        if (!resp) {
            ctx.error("No response from server (timeout)");
            return 1;
        }

        auto& r = *resp;
        if (r.value("ok", false)) {
            ctx.print("Server updated successfully.");
            std::string detail = r.value("data", nlohmann::json::object()).value("message", "");
            if (!detail.empty()) ctx.print(detail);
        } else {
            ctx.error("Error: " + r.value("error", "unknown"));
            return 1;
        }
        return 0;
    }
};

std::unique_ptr<ICommand> make_update() {
    return std::make_unique<UpdateCommand>();
}

} // namespace grotto::cli::commands
