#include "../command.hpp"
#include "../command_context.hpp"

namespace grotto::cli::commands {

class VersionCommand : public ICommand {
public:
    std::string name() const override { return "version"; }
    std::string description() const override { return "Show CLI version"; }
    std::string usage() const override { return "grotto version"; }
    CommandType type() const override { return CommandType::local; }

    int execute(CommandContext& ctx) override {
        ctx.print("grotto 0.1.0");
        return 0;
    }
};

std::unique_ptr<ICommand> make_version() {
    return std::make_unique<VersionCommand>();
}

} // namespace grotto::cli::commands
