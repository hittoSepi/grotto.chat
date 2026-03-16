#pragma once

#include "../command.hpp"
#include <memory>

namespace grotto::cli::commands {

// Factory functions for all builtin commands
std::unique_ptr<ICommand> make_help();
std::unique_ptr<ICommand> make_version();
std::unique_ptr<ICommand> make_status();
std::unique_ptr<ICommand> make_users();
std::unique_ptr<ICommand> make_shutdown();
std::unique_ptr<ICommand> make_restart();
std::unique_ptr<ICommand> make_kick();
std::unique_ptr<ICommand> make_ban();
std::unique_ptr<ICommand> make_logtail();
std::unique_ptr<ICommand> make_tui();
std::unique_ptr<ICommand> make_update();

} // namespace grotto::cli::commands
