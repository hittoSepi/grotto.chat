#include "command_registry.hpp"
#include <algorithm>

namespace grotto::cli {

void CommandRegistry::add(std::unique_ptr<ICommand> cmd) {
    auto name = cmd->name();
    commands_[name] = std::move(cmd);
}

ICommand* CommandRegistry::find(const std::string& name) const {
    auto it = commands_.find(name);
    return it != commands_.end() ? it->second.get() : nullptr;
}

std::vector<ICommand*> CommandRegistry::all() const {
    std::vector<ICommand*> result;
    result.reserve(commands_.size());
    for (const auto& [_, cmd] : commands_) {
        result.push_back(cmd.get());
    }
    std::sort(result.begin(), result.end(), [](const ICommand* a, const ICommand* b) {
        return a->name() < b->name();
    });
    return result;
}

} // namespace grotto::cli
