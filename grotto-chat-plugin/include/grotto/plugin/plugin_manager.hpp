#pragma once

#include "grotto/plugin/plugin_instance.hpp"
#include "grotto/plugin/plugin_context.hpp"

#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace grotto::plugin {

class PluginManager {
public:
    explicit PluginManager(PluginContext& ctx);
    ~PluginManager();

    // Lifecycle
    void discover(const std::filesystem::path& plugins_dir);
    std::error_code load(const std::filesystem::path& plugin_dir);
    void unload(std::string_view name);
    std::error_code reload(std::string_view name);

    // Event dispatch
    void dispatch(const PluginEvent& event);
    void dispatch_command(const PluginCommand& cmd);

    // QuickJS tick — call from host event loop
    void tick();

    // Introspection
    std::vector<PluginInfo> loaded_plugins() const;
    bool is_loaded(std::string_view name) const;
    std::vector<CommandInfo> registered_commands() const;

private:
    PluginContext& ctx_;
    std::unordered_map<std::string, std::unique_ptr<PluginInstance>> plugins_;
    mutable std::shared_mutex mu_;

    // Command -> plugin mapping for fast lookup
    std::unordered_map<std::string, PluginInstance*> command_map_;

    void rebuild_command_map();
};

} // namespace grotto::plugin
