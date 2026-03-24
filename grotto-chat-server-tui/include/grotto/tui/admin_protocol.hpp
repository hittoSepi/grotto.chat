#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace grotto::tui::protocol {

// ── Server → TUI events ────────────────────────

struct LogEntry {
    std::string level;      // "debug", "info", "warn", "error"
    std::string msg;
    std::string ts;         // ISO 8601
};

struct UserInfo {
    std::string id;
    std::string ip;
    std::string nickname;
    std::string connected;  // ISO 8601
};

struct ChannelInfo {
    std::string name;
    int members = 0;
};

struct ServerStats {
    int64_t uptime = 0;     // seconds
    int connections = 0;
    std::string version;
};

struct BugReport {
    int id = 0;
    std::string user;
    std::string description;
    std::string status;      // "new", "read", "resolved"
    std::string created;     // ISO 8601
};

struct ConfigEntry {
    std::string key;
    std::string value;
    bool read_only = false;
};

// ── TUI → Server commands ───────────────────────

struct KickCommand {
    std::string user_id;
    std::string reason;
};

struct BanCommand {
    std::string user_id;
    std::string reason;
};

struct SetConfigCommand {
    std::string key;
    std::string value;
};

struct UpdateBugReportCommand {
    int id = 0;
    std::string status;
};

// ── JSON serialization (nlohmann) ───────────────

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LogEntry, level, msg, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UserInfo, id, ip, nickname, connected)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChannelInfo, name, members)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ServerStats, uptime, connections, version)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BugReport, id, user, description, status, created)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConfigEntry, key, value, read_only)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(KickCommand, user_id, reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BanCommand, user_id, reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetConfigCommand, key, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UpdateBugReportCommand, id, status)

// ── Framing functions ───────────────────────────

/// Encode JSON to frame: 4-byte big-endian length prefix + JSON payload
std::vector<uint8_t> encode_frame(const nlohmann::json& msg);

/// Decode length prefix from buffer. Returns 0 if buffer has fewer than 4 bytes.
uint32_t decode_frame_length(const uint8_t* data, size_t len);

/// Wrap data in an event envelope: { "type": type, "data": data }
nlohmann::json to_json_event(const std::string& type, const nlohmann::json& data);

/// Wrap params in a command envelope: { "cmd": cmd, "params": params }
nlohmann::json to_json_command(const std::string& cmd, const nlohmann::json& params);

} // namespace grotto::tui::protocol
