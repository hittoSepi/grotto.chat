#pragma once

#include "command.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace grotto::cli {

class CommandRegistry {
public:
    /// Register a command. Overwrites if name already exists.
    void add(std::unique_ptr<ICommand> cmd);

    /// Find command by name. Returns nullptr if not found.
    ICommand* find(const std::string& name) const;

    /// Get all registered commands (sorted by name).
    std::vector<ICommand*> all() const;

private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> commands_;
};

} // namespace grotto::cli
