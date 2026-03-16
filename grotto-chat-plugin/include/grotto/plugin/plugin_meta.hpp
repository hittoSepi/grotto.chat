#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace grotto::plugin {

enum class PluginType {
    Bot,
    ClientExtension,
    ServerExtension,
};

enum class Permission {
    ReadMessages,
    SendMessages,
    RegisterCommands,
    ManageChannels,
    ManageUsers,
    HttpRequest,
    FileRead,
    FileWrite,
};

struct CommandDef {
    std::string name;
    std::string description;
    std::string usage;
};

struct BotConfig {
    std::string user_id;
    std::vector<std::string> auto_join_channels;
};

struct PluginMeta {
    std::string name;
    std::string display_name;
    std::string version;
    PluginType type;
    std::string entry;
    std::string description;
    std::string min_host_version;

    std::optional<BotConfig> bot;
    std::vector<CommandDef> commands;
    std::vector<Permission> permissions;

    std::filesystem::path plugin_dir;

    bool has_permission(Permission p) const;
};

// Parse plugin.json, returns nullopt on failure
std::optional<PluginMeta> parse_plugin_json(const std::filesystem::path& json_path);

// Validate that meta is consistent (e.g., bot type has bot config)
bool validate_plugin_meta(const PluginMeta& meta);

const char* plugin_type_to_string(PluginType type);
const char* permission_to_string(Permission perm);

} // namespace grotto::plugin
