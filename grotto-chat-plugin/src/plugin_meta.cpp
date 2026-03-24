#include "grotto/plugin/plugin_meta.hpp"
#include "grotto/plugin/plugin_event.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

namespace grotto::plugin {

using json = nlohmann::json;

static PluginType parse_type(const std::string& s) {
    if (s == "bot") return PluginType::Bot;
    if (s == "client_extension") return PluginType::ClientExtension;
    if (s == "server_extension") return PluginType::ServerExtension;
    throw std::invalid_argument("Unknown plugin type: " + s);
}

static Permission parse_permission(const std::string& s) {
    if (s == "read_messages") return Permission::ReadMessages;
    if (s == "send_messages") return Permission::SendMessages;
    if (s == "register_commands") return Permission::RegisterCommands;
    if (s == "manage_channels") return Permission::ManageChannels;
    if (s == "manage_users") return Permission::ManageUsers;
    if (s == "http_request") return Permission::HttpRequest;
    if (s == "file_read") return Permission::FileRead;
    if (s == "file_write") return Permission::FileWrite;
    throw std::invalid_argument("Unknown permission: " + s);
}

bool PluginMeta::has_permission(Permission p) const {
    return std::find(permissions.begin(), permissions.end(), p) != permissions.end();
}

std::optional<PluginMeta> parse_plugin_json(const std::filesystem::path& json_path) {
    try {
        std::ifstream f(json_path);
        if (!f.is_open()) {
            spdlog::error("Cannot open plugin.json: {}", json_path.string());
            return std::nullopt;
        }

        auto j = json::parse(f);
        PluginMeta meta;

        meta.name = j.at("name").get<std::string>();
        meta.display_name = j.value("display_name", meta.name);
        meta.version = j.at("version").get<std::string>();
        meta.type = parse_type(j.at("type").get<std::string>());
        meta.entry = j.at("entry").get<std::string>();
        meta.description = j.value("description", "");
        meta.min_host_version = j.value("min_host_version", "0.0.0");
        meta.plugin_dir = json_path.parent_path();

        // Bot config
        if (j.contains("bot")) {
            BotConfig bc;
            auto& bot_j = j["bot"];
            bc.user_id = bot_j.at("user_id").get<std::string>();
            if (bot_j.contains("auto_join_channels")) {
                bc.auto_join_channels = bot_j["auto_join_channels"]
                    .get<std::vector<std::string>>();
            }
            meta.bot = std::move(bc);
        }

        // Commands
        if (j.contains("commands")) {
            for (auto& cmd_j : j["commands"]) {
                CommandDef cd;
                cd.name = cmd_j.at("name").get<std::string>();
                cd.description = cmd_j.value("description", "");
                cd.usage = cmd_j.value("usage", "");
                meta.commands.push_back(std::move(cd));
            }
        }

        // Permissions
        if (j.contains("permissions")) {
            for (auto& perm_s : j["permissions"]) {
                meta.permissions.push_back(parse_permission(perm_s.get<std::string>()));
            }
        }

        return meta;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse plugin.json {}: {}", json_path.string(), e.what());
        return std::nullopt;
    }
}

bool validate_plugin_meta(const PluginMeta& meta) {
    if (meta.name.empty()) {
        spdlog::error("Plugin name is empty");
        return false;
    }
    if (meta.entry.empty()) {
        spdlog::error("Plugin entry is empty for '{}'", meta.name);
        return false;
    }
    if (meta.type == PluginType::Bot && !meta.bot) {
        spdlog::error("Bot plugin '{}' missing 'bot' config section", meta.name);
        return false;
    }

    // Validate permissions vs type
    for (auto p : meta.permissions) {
        if (meta.type == PluginType::ServerExtension) {
            if (p == Permission::ReadMessages || p == Permission::SendMessages ||
                p == Permission::RegisterCommands) {
                spdlog::error("Server extension '{}' cannot have permission '{}'",
                              meta.name, permission_to_string(p));
                return false;
            }
        }
        if (meta.type != PluginType::ServerExtension) {
            if (p == Permission::ManageChannels || p == Permission::ManageUsers) {
                spdlog::error("Plugin '{}' (type {}) cannot have permission '{}'",
                              meta.name, plugin_type_to_string(meta.type),
                              permission_to_string(p));
                return false;
            }
        }
    }

    return true;
}

const char* plugin_type_to_string(PluginType type) {
    switch (type) {
        case PluginType::Bot: return "bot";
        case PluginType::ClientExtension: return "client_extension";
        case PluginType::ServerExtension: return "server_extension";
    }
    return "unknown";
}

const char* permission_to_string(Permission perm) {
    switch (perm) {
        case Permission::ReadMessages: return "read_messages";
        case Permission::SendMessages: return "send_messages";
        case Permission::RegisterCommands: return "register_commands";
        case Permission::ManageChannels: return "manage_channels";
        case Permission::ManageUsers: return "manage_users";
        case Permission::HttpRequest: return "http_request";
        case Permission::FileRead: return "file_read";
        case Permission::FileWrite: return "file_write";
    }
    return "unknown";
}

const char* event_type_to_string(PluginEvent::Type type) {
    switch (type) {
        case PluginEvent::Type::UserJoined: return "user_joined";
        case PluginEvent::Type::UserLeft: return "user_left";
        case PluginEvent::Type::PresenceChanged: return "presence_changed";
        case PluginEvent::Type::ChannelCreated: return "channel_created";
        case PluginEvent::Type::ChannelDeleted: return "channel_deleted";
        case PluginEvent::Type::MessageReceived: return "message";
        case PluginEvent::Type::ConnectionEstablished: return "connection_established";
        case PluginEvent::Type::ConnectionDropped: return "connection_dropped";
        case PluginEvent::Type::VoiceRoomJoined: return "voice_room_joined";
        case PluginEvent::Type::VoiceRoomLeft: return "voice_room_left";
        case PluginEvent::Type::AuthSuccess: return "auth_success";
        case PluginEvent::Type::AuthFailure: return "auth_failure";
    }
    return "unknown";
}

} // namespace grotto::plugin
