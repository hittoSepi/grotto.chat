#include "grotto/plugin/plugin_manager.hpp"
#include "grotto/plugin/bot_plugin_instance.hpp"
#include "grotto/plugin/client_ext_plugin.hpp"
#include "grotto/plugin/server_ext_plugin.hpp"

#include <spdlog/spdlog.h>

namespace grotto::plugin {

PluginManager::PluginManager(PluginContext& ctx) : ctx_(ctx) {}

PluginManager::~PluginManager() {
    std::unique_lock lock(mu_);
    for (auto& [name, plugin] : plugins_) {
        plugin->shutdown();
    }
    plugins_.clear();
    command_map_.clear();
}

void PluginManager::discover(const std::filesystem::path& plugins_dir) {
    if (!std::filesystem::exists(plugins_dir)) {
        spdlog::warn("Plugin directory does not exist: {}", plugins_dir.string());
        return;
    }

    for (auto& entry : std::filesystem::directory_iterator(plugins_dir)) {
        if (!entry.is_directory()) continue;

        auto json_path = entry.path() / "plugin.json";
        if (!std::filesystem::exists(json_path)) continue;

        auto ec = load(entry.path());
        if (ec) {
            spdlog::error("Failed to load plugin from {}: {}",
                          entry.path().string(), ec.message());
        }
    }
}

std::error_code PluginManager::load(const std::filesystem::path& plugin_dir) {
    auto json_path = plugin_dir / "plugin.json";
    auto meta_opt = parse_plugin_json(json_path);
    if (!meta_opt) return PluginError::InvalidMeta;

    auto& meta = *meta_opt;
    if (!validate_plugin_meta(meta)) return PluginError::InvalidMeta;

    {
        std::shared_lock lock(mu_);
        if (plugins_.contains(meta.name)) {
            spdlog::warn("Plugin '{}' already loaded", meta.name);
            return PluginError::AlreadyLoaded;
        }
    }

    // Create appropriate subclass
    std::unique_ptr<PluginInstance> instance;
    switch (meta.type) {
        case PluginType::Bot:
            instance = std::make_unique<BotPluginInstance>(std::move(meta), ctx_);
            break;
        case PluginType::ClientExtension:
            instance = std::make_unique<ClientExtPlugin>(std::move(meta), ctx_);
            break;
        case PluginType::ServerExtension:
            instance = std::make_unique<ServerExtPlugin>(std::move(meta), ctx_);
            break;
    }

    auto ec = instance->init();
    if (ec) return ec;

    std::unique_lock lock(mu_);
    auto name = instance->meta().name;
    plugins_[name] = std::move(instance);
    rebuild_command_map();
    return PluginError::None;
}

void PluginManager::unload(std::string_view name) {
    std::unique_lock lock(mu_);
    auto it = plugins_.find(std::string(name));
    if (it == plugins_.end()) {
        spdlog::warn("Cannot unload '{}': not found", name);
        return;
    }

    it->second->shutdown();
    plugins_.erase(it);
    rebuild_command_map();
    spdlog::info("Plugin '{}' unloaded", name);
}

std::error_code PluginManager::reload(std::string_view name) {
    std::filesystem::path plugin_dir;

    {
        std::shared_lock lock(mu_);
        auto it = plugins_.find(std::string(name));
        if (it == plugins_.end()) return PluginError::NotFound;
        plugin_dir = it->second->meta().plugin_dir;
    }

    unload(name);
    return load(plugin_dir);
}

void PluginManager::dispatch(const PluginEvent& event) {
    std::shared_lock lock(mu_);

    for (auto& [name, plugin] : plugins_) {
        // Server extensions never get plaintext message events
        if (plugin->meta().type == PluginType::ServerExtension &&
            event.type == PluginEvent::Type::MessageReceived) {
            continue;
        }

        plugin->on_event(event);
    }
}

void PluginManager::dispatch_command(const PluginCommand& cmd) {
    std::shared_lock lock(mu_);

    auto it = command_map_.find(cmd.name);
    if (it == command_map_.end()) return;

    it->second->on_command(cmd);
}

void PluginManager::tick() {
    std::shared_lock lock(mu_);

    // Collect error-state plugins for unloading
    std::vector<std::string> to_unload;

    for (auto& [name, plugin] : plugins_) {
        plugin->tick();
        if (plugin->state() == PluginState::Error) {
            to_unload.push_back(name);
        }
    }

    lock.unlock();

    // Unload error-state plugins
    for (auto& name : to_unload) {
        unload(name);
    }
}

std::vector<PluginInfo> PluginManager::loaded_plugins() const {
    std::shared_lock lock(mu_);
    std::vector<PluginInfo> result;
    for (auto& [name, plugin] : plugins_) {
        result.push_back(plugin->info());
    }
    return result;
}

bool PluginManager::is_loaded(std::string_view name) const {
    std::shared_lock lock(mu_);
    return plugins_.contains(std::string(name));
}

std::vector<CommandInfo> PluginManager::registered_commands() const {
    std::shared_lock lock(mu_);
    std::vector<CommandInfo> result;
    for (auto& [name, plugin] : plugins_) {
        auto cmds = plugin->registered_commands();
        result.insert(result.end(), cmds.begin(), cmds.end());
    }
    return result;
}

void PluginManager::rebuild_command_map() {
    command_map_.clear();
    for (auto& [name, plugin] : plugins_) {
        for (auto& ci : plugin->registered_commands()) {
            command_map_[ci.name] = plugin.get();
        }
    }
}

} // namespace grotto::plugin
