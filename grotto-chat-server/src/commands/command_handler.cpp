#include "commands/command_handler.hpp"
#include "net/session.hpp"
#include "db/database.hpp"
#include "db/user_store.hpp"
#include "db/offline_store.hpp"
#include "db/file_store.hpp"
#include "utils/channel_utils.hpp"
#include "utils/nickname_utils.hpp"
#include <spdlog/spdlog.h>
#include <sodium.h>
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

namespace grotto::commands {

namespace {

std::string format_identity_fingerprint(const std::vector<uint8_t>& identity_pub) {
    if (identity_pub.empty()) {
        return {};
    }

    std::string fingerprint(identity_pub.size() * 2 + 1, '\0');
    sodium_bin2hex(fingerprint.data(), fingerprint.size(), identity_pub.data(), identity_pub.size());
    fingerprint.pop_back();
    return fingerprint;
}

std::string resolve_target_user_id(
    const std::unordered_map<std::string, std::string>& nick_to_user_id,
    const std::string& target) {
    if (const auto exact = nick_to_user_id.find(target); exact != nick_to_user_id.end()) {
        return exact->second;
    }

    const auto it = std::find_if(
        nick_to_user_id.begin(),
        nick_to_user_id.end(),
        [&target](const auto& entry) {
            return utils::nicknames_equal(entry.first, target);
        });

    return it != nick_to_user_id.end() ? it->second : target;
}

std::string normalize_channel_argument(const std::string& input) {
    const std::string channel = utils::sanitize_channel_name(input);
    return utils::is_valid_channel_name(channel) ? channel : std::string();
}

std::string human_bytes(uint64_t bytes) {
    static constexpr std::array<const char*, 5> kUnits = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    std::size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < kUnits.size()) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream oss;
    if (unit_index == 0) {
        oss << bytes << ' ' << kUnits[unit_index];
    } else if (value >= 10.0) {
        oss << std::fixed << std::setprecision(0) << value << ' ' << kUnits[unit_index];
    } else {
        oss << std::fixed << std::setprecision(1) << value << ' ' << kUnits[unit_index];
    }
    return oss.str();
}

std::string quota_limit_label(uint64_t limit) {
    return limit == 0 ? std::string("unlimited") : human_bytes(limit);
}

std::string quota_remaining_label(uint64_t used, uint64_t limit) {
    if (limit == 0) {
        return "unlimited";
    }
    const auto remaining = (used >= limit) ? uint64_t{0} : (limit - used);
    return human_bytes(remaining);
}

std::string join_command_args(const std::vector<std::string>& args) {
    std::string joined;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            joined += ' ';
        }
        joined += args[i];
    }
    return joined;
}

} // namespace

// Helper to create CommandResponse
inline CommandResponse make_response(bool success, const std::string& message, const std::string& command) {
    CommandResponse resp;
    resp.set_success(success);
    resp.set_message(message);
    resp.set_command(command);
    return resp;
}

CommandHandler::CommandHandler(
    SessionFinder find_session,
    BroadcastFunc broadcast,
    PresenceUpdateFn update_presence,
    PresenceLookupFn lookup_presence,
    db::Database& db,
    db::UserStore& user_store,
    db::OfflineStore& offline_store,
    db::FileStore& file_store,
    uint64_t max_total_storage_bytes,
    uint64_t max_user_storage_bytes)
    : find_session_(std::move(find_session))
    , broadcast_(std::move(broadcast))
    , update_presence_(std::move(update_presence))
    , lookup_presence_(std::move(lookup_presence))
    , db_(db)
    , user_store_(user_store)
    , offline_store_(offline_store)
    , file_store_(file_store)
    , max_total_storage_bytes_(max_total_storage_bytes)
    , max_user_storage_bytes_(max_user_storage_bytes)
{
    // Register commands
    command_map_["join"] = [this](auto& args, auto s) { return cmd_join(args, s); };
    command_map_["part"] = [this](auto& args, auto s) { return cmd_part(args, s); };
    command_map_["leave"] = [this](auto& args, auto s) { return cmd_part(args, s); };
    command_map_["nick"] = [this](auto& args, auto s) { return cmd_nick(args, s); };
    command_map_["whois"] = [this](auto& args, auto s) { return cmd_whois(args, s); };
    command_map_["me"] = [this](auto& args, auto s) { return cmd_me(args, s); };
    command_map_["action"] = [this](auto& args, auto s) { return cmd_me(args, s); };
    command_map_["topic"] = [this](auto& args, auto s) { return cmd_topic(args, s); };
    command_map_["kick"] = [this](auto& args, auto s) { return cmd_kick(args, s); };
    command_map_["ban"] = [this](auto& args, auto s) { return cmd_ban(args, s); };
    command_map_["invite"] = [this](auto& args, auto s) { return cmd_invite(args, s); };
    command_map_["set"] = [this](auto& args, auto s) { return cmd_set(args, s); };
    command_map_["mode"] = [this](auto& args, auto s) { return cmd_mode(args, s); };
    command_map_["password"] = [this](auto& args, auto s) { return cmd_password(args, s); };
    command_map_["pass"] = [this](auto& args, auto s) { return cmd_password(args, s); };
    command_map_["quit"] = [this](auto& args, auto s) { return cmd_quit(args, s); };
    command_map_["msg"] = [this](auto& args, auto s) { return cmd_msg(args, s); };
    command_map_["query"] = [this](auto& args, auto s) { return cmd_msg(args, s); };
    command_map_["away"] = [this](auto& args, auto s) { return cmd_away(args, s); };
    command_map_["back"] = [this](auto& args, auto s) { return cmd_back(args, s); };
    command_map_["dnd"] = [this](auto& args, auto s) { return cmd_dnd(args, s); };
    command_map_["quota"] = [this](auto& args, auto s) { return cmd_quota(args, s); };
    command_map_["rmfile"] = [this](auto& args, auto s) { return cmd_rmfile(args, s); };
    command_map_["resetdb"] = [this](auto& args, auto s) { return cmd_resetdb(args, s); };
}

CommandResponse CommandHandler::handle_command(const IrcCommand& cmd, SessionPtr session) {
    if (cmd.command().empty()) {
        return make_response(false, "Empty command", "");
    }
    
    const std::string& user_id = session->user_id();
    
    // Check if user is banned for abuse
    if (is_abuser(user_id)) {
        return make_response(false, "You are temporarily banned due to rate limit violations. Please wait.", cmd.command());
    }
    
    // Get or create rate limits for this user
    UserRateLimits* limits = nullptr;
    {
        std::lock_guard<std::mutex> lk(rate_limit_mutex_);
        auto [it, inserted] = user_rate_limits_.emplace(user_id, UserRateLimits{});
        limits = &it->second;
    }
    
    // Check rate limit (30 commands per minute)
    if (!limits->command_limiter.allow()) {
        if (track_abuse(user_id)) {
            spdlog::warn("User {} banned for repeated command rate limit violations", user_id);
        }
        return make_response(false, "Rate limit exceeded: too many commands. Slow down.", cmd.command());
    }

    std::string cmd_name = cmd.command();
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::tolower);

    auto it = command_map_.find(cmd_name);
    if (it == command_map_.end()) {
        return make_response(false, "Unknown command: " + cmd_name, cmd_name);
    }

    std::vector<std::string> args(cmd.args().begin(), cmd.args().end());
    return it->second(args, session);
}

ChannelState& CommandHandler::get_or_create_channel(const std::string& name) {
    auto it = channels_.find(name);
    if (it == channels_.end()) {
        ChannelState state;
        state.name = name;
        state.topic = "";
        auto [inserted_it, _] = channels_.emplace(name, std::move(state));
        return inserted_it->second;
    }
    return it->second;
}

CommandResponse CommandHandler::cmd_join(const std::vector<std::string>& args, SessionPtr session) {
    if (args.empty()) {
        return make_response(false, "Usage: /join <channel>", "join");
    }

    // Sanitize channel name: auto-add # prefix, strip invalid chars
    std::string channel = utils::sanitize_channel_name(args[0]);
    
    // Validate the sanitized channel has content beyond just the prefix
    if (!utils::is_valid_channel_name(channel)) {
        return make_response(false, "Invalid channel name", "join");
    }

    const std::string& user_id = session->user_id();
    
    // Check join rate limit (5 joins per minute)
    UserRateLimits* limits = nullptr;
    {
        std::lock_guard<std::mutex> lk(rate_limit_mutex_);
        auto [it, inserted] = user_rate_limits_.emplace(user_id, UserRateLimits{});
        limits = &it->second;
    }
    
    if (!limits->join_limiter.allow()) {
        if (track_abuse(user_id)) {
            spdlog::warn("User {} banned for repeated join rate limit violations", user_id);
        }
        return make_response(false, "Rate limit exceeded: too many channel joins. Slow down.", "join");
    }
    
    auto& chan = get_or_create_channel(channel);

    if (chan.banned.count(user_id)) {
        return make_response(false, "You are banned from " + channel, "join");
    }

    if (chan.invite_only && !chan.invites.count(user_id) && !chan.operators.count(user_id)) {
        return make_response(false, channel + " is invite-only", "join");
    }

    chan.members.insert(user_id);
    
    if (chan.members.size() == 1) {
        chan.operators.insert(user_id);
        spdlog::info("{} became operator of {}", user_id, channel);
    }

    notify_channel_join(channel, user_id);
    send_user_list(channel, session);

    std::string topic_msg = chan.topic.empty() ? "No topic set" : "Topic: " + chan.topic;
    return make_response(true, "Joined " + channel + "\n" + topic_msg, "join");
}

CommandResponse CommandHandler::cmd_part(const std::vector<std::string>& args, SessionPtr session) {
    const std::string& user_id = session->user_id();
    
    if (args.empty()) {
        return make_response(false, "Usage: /part <channel> [message]", "part");
    }

    std::string channel = normalize_channel_argument(args[0]);
    if (channel.empty()) {
        return make_response(false, "Invalid channel name", "part");
    }
    std::string reason = args.size() > 1 ? args[1] : "Leaving";

    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return make_response(false, "No such channel: " + channel, "part");
    }

    auto& chan = it->second;
    if (!chan.members.count(user_id)) {
        return make_response(false, "You are not in " + channel, "part");
    }

    chan.members.erase(user_id);
    chan.operators.erase(user_id);
    chan.voiced.erase(user_id);

    notify_channel_part(channel, user_id, reason);

    if (chan.members.empty()) {
        channels_.erase(it);
        spdlog::info("{} was destroyed (empty)", channel);
    }

    return make_response(true, "Left " + channel, "part");
}

CommandResponse CommandHandler::cmd_nick(const std::vector<std::string>& args, SessionPtr session) {
    if (args.empty()) {
        return make_response(false, "Usage: /nick <new_nick>", "nick");
    }

    std::string new_nick = args[0];
    const std::string& user_id = session->user_id();
    std::string old_nick = user_id_to_nick_.count(user_id) ? user_id_to_nick_[user_id] : user_id;

    const std::string existing_user_id = resolve_target_user_id(nick_to_user_id_, new_nick);
    if (existing_user_id != new_nick && existing_user_id != user_id) {
        return make_response(false, "Nickname " + new_nick + " is already in use", "nick");
    }

    // Remove old mapping
    nick_to_user_id_.erase(old_nick);
    
    // Set new mapping
    nick_to_user_id_[new_nick] = user_id;
    user_id_to_nick_[user_id] = new_nick;

    // Broadcast nick change to all channels user is in
    NickChange nick_change;
    nick_change.set_old_nick(old_nick);
    nick_change.set_new_nick(new_nick);

    for (auto& [chan_name, chan] : channels_) {
        if (chan.members.count(user_id)) {
            Envelope env;
            env.set_type(MT_NICK_CHANGE);
            nick_change.SerializeToString(env.mutable_payload());
            broadcast_to_channel(chan_name, env);
        }
    }

    spdlog::info("{} changed nick to {}", user_id, new_nick);
    return make_response(true, "You are now known as " + new_nick, "nick");
}

CommandResponse CommandHandler::cmd_whois(const std::vector<std::string>& args, SessionPtr /*session*/) {
    if (args.empty()) {
        return make_response(false, "Usage: /whois <nick>", "whois");
    }

    std::string target = args[0];
    std::string target_id;

    // Look up by nick or user_id
    target_id = resolve_target_user_id(nick_to_user_id_, target);

    const std::string nickname =
        user_id_to_nick_.count(target_id) ? user_id_to_nick_[target_id] : target_id;
    PresenceUpdate::Status presence_status = PresenceUpdate::OFFLINE;
    std::string status_text;
    int64_t status_since_ms = 0;

    // Add channels
    std::vector<std::string> channels;
    for (const auto& [chan_name, chan] : channels_) {
        if (chan.members.count(target_id)) {
            channels.push_back(chan_name);
        }
    }

    std::string fingerprint;
    if (auto user = user_store_.find_by_id(target_id)) {
        fingerprint = format_identity_fingerprint(user->identity_pub);
    }

    if (lookup_presence_) {
        if (const auto presence = lookup_presence_(target_id)) {
            presence_status = presence->status;
            status_text = presence->status_text;
            status_since_ms = presence->status_since_ms;
        }
    }

    std::string status = "offline";
    if (presence_status == PresenceUpdate::ONLINE) {
        status = "online";
    } else if (presence_status == PresenceUpdate::AWAY) {
        status = "away";
    } else if (presence_status == PresenceUpdate::DND) {
        status = "do not disturb";
    }

    std::ostringstream out;
    out << "Whois for " << nickname << ":\n";
    out << "User ID: " << target_id << "\n";
    out << "Status: " << status << "\n";
    if (!status_text.empty()) {
        out << "Status text: " << status_text << "\n";
    }
    if (status_since_ms > 0) {
        out << "Status since ms: " << status_since_ms << "\n";
    }
    if (!channels.empty()) {
        out << "Channels: ";
        for (size_t i = 0; i < channels.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << channels[i];
        }
        out << "\n";
    } else {
        out << "Channels: (none)\n";
    }
    if (!fingerprint.empty()) {
        out << "Fingerprint: " << fingerprint;
    }

    return make_response(true, out.str(), "whois");
}

CommandResponse CommandHandler::cmd_me(const std::vector<std::string>& args, SessionPtr session) {
    if (args.empty()) {
        return make_response(false, "Usage: /me <action>", "me");
    }

    std::string action = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        action += " " + args[i];
    }

    // Action messages are sent as special chat envelopes
    // The client handles rendering them differently
    // For now, just acknowledge
    
    return make_response(true, "Action: " + session->user_id() + " " + action, "me");
}

CommandResponse CommandHandler::cmd_topic(const std::vector<std::string>& args, SessionPtr session) {
    if (args.empty()) {
        return make_response(false, "Usage: /topic <channel> [new_topic]", "topic");
    }

    std::string channel = normalize_channel_argument(args[0]);
    if (channel.empty()) {
        return make_response(false, "Invalid channel name", "topic");
    }
    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return make_response(false, "No such channel: " + channel, "topic");
    }

    auto& chan = it->second;
    const std::string& user_id = session->user_id();

    if (!chan.members.count(user_id)) {
        return make_response(false, "You are not in " + channel, "topic");
    }

    // If no topic provided, just show current
    if (args.size() < 2) {
        std::string topic = chan.topic.empty() ? "No topic set" : chan.topic;
        return make_response(true, topic, "topic");
    }

    // Check permission
    if (chan.topic_restricted && !chan.operators.count(user_id)) {
        return make_response(false, "Only operators can change the topic", "topic");
    }

    // Set topic
    chan.topic = args[1];
    for (size_t i = 2; i < args.size(); ++i) {
        chan.topic += " " + args[i];
    }

    // Broadcast topic change
    notify_channel_join(channel, user_id); // Reuse join notification for now

    return make_response(true, "Topic changed to: " + chan.topic, "topic");
}

CommandResponse CommandHandler::cmd_kick(const std::vector<std::string>& args, SessionPtr session) {
    if (args.size() < 2) {
        return make_response(false, "Usage: /kick <channel> <nick> [reason]", "kick");
    }

    std::string channel = normalize_channel_argument(args[0]);
    if (channel.empty()) {
        return make_response(false, "Invalid channel name", "kick");
    }
    std::string target = args[1];
    std::string reason = args.size() > 2 ? args[2] : "Kicked";

    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return make_response(false, "No such channel: " + channel, "kick");
    }

    auto& chan = it->second;
    const std::string& user_id = session->user_id();

    if (!chan.operators.count(user_id)) {
        return make_response(false, "Only operators can kick users", "kick");
    }

    // Look up target
    std::string target_id;
    target_id = resolve_target_user_id(nick_to_user_id_, target);
    if (target_id == target && chan.members.count(target)) {
        target_id = target;
    }

    if (target_id == target && !chan.members.count(target_id)) {
        return make_response(false, "User not found: " + target, "kick");
    }

    if (!chan.members.count(target_id)) {
        return make_response(false, target + " is not in " + channel, "kick");
    }

    // Remove from channel
    chan.members.erase(target_id);
    chan.operators.erase(target_id);
    chan.voiced.erase(target_id);

    notify_channel_part(channel, target_id, "Kicked by " + user_id + ": " + reason);

    // Notify the kicked user
    auto target_session = find_session_(target_id);
    if (target_session) {
        auto kick_msg = make_response(false, "You were kicked from " + channel + " by " + user_id + ": " + reason, "kick");
        Envelope env;
        env.set_type(MT_COMMAND_RESPONSE);
        kick_msg.SerializeToString(env.mutable_payload());
        target_session->send(env);
    }

    return make_response(true, "Kicked " + target + " from " + channel, "kick");
}

CommandResponse CommandHandler::cmd_ban(const std::vector<std::string>& args, SessionPtr session) {
    if (args.size() < 2) {
        return make_response(false, "Usage: /ban <channel> <nick>", "ban");
    }

    std::string channel = normalize_channel_argument(args[0]);
    if (channel.empty()) {
        return make_response(false, "Invalid channel name", "ban");
    }
    std::string target = args[1];

    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return make_response(false, "No such channel: " + channel, "ban");
    }

    auto& chan = it->second;
    const std::string& user_id = session->user_id();

    if (!chan.operators.count(user_id)) {
        return make_response(false, "Only operators can ban users", "ban");
    }

    std::string target_id = resolve_target_user_id(nick_to_user_id_, target);
    chan.banned.insert(target_id);

    // Kick if currently in channel
    if (chan.members.count(target_id)) {
        chan.members.erase(target_id);
        notify_channel_part(channel, target_id, "Banned by " + user_id);
    }

    return make_response(true, "Banned " + target + " from " + channel, "ban");
}

CommandResponse CommandHandler::cmd_invite(const std::vector<std::string>& args, SessionPtr session) {
    if (args.size() < 2) {
        return make_response(false, "Usage: /invite <channel> <nick> [message]", "invite");
    }

    std::string channel = normalize_channel_argument(args[0]);
    if (channel.empty()) {
        return make_response(false, "Invalid channel name", "invite");
    }
    std::string target = args[1];
    std::string message = args.size() > 2 ? args[2] : "";

    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return make_response(false, "No such channel: " + channel, "invite");
    }

    auto& chan = it->second;
    const std::string& user_id = session->user_id();

    if (!chan.operators.count(user_id)) {
        return make_response(false, "Only operators can invite users", "invite");
    }

    std::string target_id = resolve_target_user_id(nick_to_user_id_, target);
    chan.invites.insert(target_id);

    // Notify target if online
    auto target_session = find_session_(target_id);
    if (target_session) {
        auto invite_msg = make_response(true, user_id + " invited you to " + channel + 
            (message.empty() ? "" : ": " + message), "invite");
        Envelope env;
        env.set_type(MT_COMMAND_RESPONSE);
        invite_msg.SerializeToString(env.mutable_payload());
        target_session->send(env);
    }

    return make_response(true, "Invited " + target + " to " + channel, "invite");
}

CommandResponse CommandHandler::cmd_set(const std::vector<std::string>& args, SessionPtr session) {
    if (args.size() < 3) {
        return make_response(false, "Usage: /set <channel> <option> <value>", "set");
    }

    std::string channel = normalize_channel_argument(args[0]);
    if (channel.empty()) {
        return make_response(false, "Invalid channel name", "set");
    }
    std::string option = args[1];
    std::string value = args[2];

    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return make_response(false, "No such channel: " + channel, "set");
    }

    auto& chan = it->second;
    const std::string& user_id = session->user_id();

    if (!chan.operators.count(user_id)) {
        return make_response(false, "Only operators can change channel settings", "set");
    }

    std::transform(option.begin(), option.end(), option.begin(), ::tolower);

    if (option == "invite_only") {
        chan.invite_only = (value == "true" || value == "1" || value == "on");
        return make_response(true, "invite_only set to " + std::string(chan.invite_only ? "true" : "false"), "set");
    } else if (option == "moderated") {
        chan.moderated = (value == "true" || value == "1" || value == "on");
        return make_response(true, "moderated set to " + std::string(chan.moderated ? "true" : "false"), "set");
    } else if (option == "topic_restricted") {
        chan.topic_restricted = (value == "true" || value == "1" || value == "on");
        return make_response(true, "topic_restricted set to " + std::string(chan.topic_restricted ? "true" : "false"), "set");
    }

    return make_response(false, "Unknown option: " + option, "set");
}

CommandResponse CommandHandler::cmd_mode(const std::vector<std::string>& args, SessionPtr session) {
    if (args.size() < 3) {
        return make_response(false, "Usage: /mode <channel> <+o|-o> <nick>", "mode");
    }

    std::string channel = normalize_channel_argument(args[0]);
    if (channel.empty()) {
        return make_response(false, "Invalid channel name", "mode");
    }
    std::string mode = args[1];
    std::string target = args[2];

    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return make_response(false, "No such channel: " + channel, "mode");
    }

    auto& chan = it->second;
    const std::string& user_id = session->user_id();

    if (!chan.operators.count(user_id)) {
        return make_response(false, "Only operators can change modes", "mode");
    }

    std::string target_id = resolve_target_user_id(nick_to_user_id_, target);
    
    if (!chan.members.count(target_id)) {
        return make_response(false, target + " is not in " + channel, "mode");
    }

    if (mode == "+o") {
        chan.operators.insert(target_id);
        return make_response(true, "Gave operator status to " + target, "mode");
    } else if (mode == "-o") {
        chan.operators.erase(target_id);
        return make_response(true, "Removed operator status from " + target, "mode");
    } else if (mode == "+v") {
        chan.voiced.insert(target_id);
        return make_response(true, "Gave voice to " + target, "mode");
    } else if (mode == "-v") {
        chan.voiced.erase(target_id);
        return make_response(true, "Removed voice from " + target, "mode");
    }

    return make_response(false, "Unknown mode: " + mode, "mode");
}

// ============================================================================
// Helper functions
// ============================================================================

void CommandHandler::notify_channel_join(const std::string& channel, const std::string& /*user_id*/) {
    broadcast_user_list(channel);
}

void CommandHandler::notify_channel_part(const std::string& channel, const std::string& /*user_id*/, const std::string& /*reason*/) {
    broadcast_user_list(channel);
}

void CommandHandler::send_user_list(const std::string& channel, SessionPtr session) {
    auto it = channels_.find(channel);
    if (it == channels_.end()) return;

    auto& chan = it->second;
    std::string user_list = "Users in " + channel + ":\n";
    
    for (const auto& uid : chan.members) {
        std::string prefix = chan.operators.count(uid) ? "@" : (chan.voiced.count(uid) ? "+" : "");
        std::string nick = user_id_to_nick_.count(uid) ? user_id_to_nick_[uid] : uid;
        user_list += prefix + nick + "\n";
    }

    auto response = make_response(true, user_list, "names");
    Envelope env;
    env.set_type(MT_COMMAND_RESPONSE);
    response.SerializeToString(env.mutable_payload());
    session->send(env);
}

void CommandHandler::broadcast_user_list(const std::string& channel) {
    auto it = channels_.find(channel);
    if (it == channels_.end()) return;

    auto& chan = it->second;
    std::string user_list = "Users in " + channel + ":\n";

    for (const auto& uid : chan.members) {
        std::string prefix = chan.operators.count(uid) ? "@" : (chan.voiced.count(uid) ? "+" : "");
        std::string nick = user_id_to_nick_.count(uid) ? user_id_to_nick_[uid] : uid;
        user_list += prefix + nick + "\n";
    }

    auto response = make_response(true, user_list, "names");
    Envelope env;
    env.set_type(MT_COMMAND_RESPONSE);
    response.SerializeToString(env.mutable_payload());

    // Send to all online channel members
    for (const auto& uid : chan.members) {
        auto session = find_session_(uid);
        if (session) {
            session->send(env);
        }
    }
}

void CommandHandler::broadcast_to_channel(const std::string& channel, const Envelope& env, SessionPtr exclude) {
    auto it = channels_.find(channel);
    if (it == channels_.end()) return;

    // Serialize envelope once for offline storage
    std::vector<uint8_t> serialized_payload;
    bool serialized = false;

    for (const auto& user_id : it->second.members) {
        auto session = find_session_(user_id);
        if (session && session != exclude) {
            // User is online - send immediately
            session->send(env);
        } else if (!session) {
            // User is offline - store for later delivery
            if (!serialized) {
                serialized_payload.resize(env.ByteSizeLong());
                env.SerializeToArray(serialized_payload.data(), static_cast<int>(serialized_payload.size()));
                serialized = true;
            }
            bool saved = offline_store_.save(user_id, serialized_payload);
            if (saved) {
                spdlog::debug("CommandHandler: stored channel message for offline user {}", user_id);
            } else {
                spdlog::warn("CommandHandler: failed to store message for {} (queue full)", user_id);
            }
        }
    }
}

bool CommandHandler::is_in_channel(const std::string& channel, const std::string& user_id) {
    auto it = channels_.find(channel);
    return it != channels_.end() && it->second.members.count(user_id);
}

bool CommandHandler::is_operator(const std::string& channel, const std::string& user_id) {
    auto it = channels_.find(channel);
    return it != channels_.end() && it->second.operators.count(user_id);
}

bool CommandHandler::is_voiced(const std::string& channel, const std::string& user_id) {
    auto it = channels_.find(channel);
    return it != channels_.end() && it->second.voiced.count(user_id);
}

std::vector<std::string> CommandHandler::get_channel_members(const std::string& channel) {
    auto it = channels_.find(channel);
    if (it == channels_.end()) return {};
    return std::vector<std::string>(it->second.members.begin(), it->second.members.end());
}

std::string CommandHandler::get_channel_topic(const std::string& channel) {
    auto it = channels_.find(channel);
    return it != channels_.end() ? it->second.topic : "";
}

void CommandHandler::join_channel(const std::string& channel, const std::string& user_id) {
    auto& chan = get_or_create_channel(channel);
    chan.members.insert(user_id);
    if (chan.members.size() == 1) {
        chan.operators.insert(user_id);
    }
}

void CommandHandler::part_channel(const std::string& channel, const std::string& user_id) {
    auto it = channels_.find(channel);
    if (it == channels_.end()) return;
    
    it->second.members.erase(user_id);
    it->second.operators.erase(user_id);
    it->second.voiced.erase(user_id);
    
    if (it->second.members.empty()) {
        channels_.erase(it);
    }
}

// ============================================================================
// /password <old_password> <new_password>
// ============================================================================
CommandResponse CommandHandler::cmd_password(const std::vector<std::string>& args, SessionPtr session) {
    if (args.size() < 2) {
        return make_response(false, "Usage: /password <old_password> <new_password>", "password");
    }

    const std::string& user_id = session->user_id();
    const std::string& old_pass = args[0];
    const std::string& new_pass = args[1];

    // Validate new password length
    if (new_pass.length() < 8) {
        return make_response(false, "New password must be at least 8 characters", "password");
    }
    
    // Validate password complexity (optional but recommended)
    bool has_upper = false, has_lower = false, has_digit = false;
    for (char c : new_pass) {
        if (std::isupper(c)) has_upper = true;
        if (std::islower(c)) has_lower = true;
        if (std::isdigit(c)) has_digit = true;
    }
    
    if (!has_upper || !has_lower || !has_digit) {
        return make_response(false, 
            "Password must contain at least one uppercase letter, one lowercase letter, and one digit", 
            "password");
    }

    // Update password in database
    if (user_store_.update_password(user_id, old_pass, new_pass)) {
        spdlog::info("Password changed successfully for user {}", user_id);
        return make_response(true, "Password updated successfully", "password");
    } else {
        spdlog::warn("Password change failed for user {}: old password incorrect", user_id);
        return make_response(false, "Old password is incorrect", "password");
    }
}

// ============================================================================
// /quit [message]
// ============================================================================
CommandResponse CommandHandler::cmd_quit(const std::vector<std::string>& args, SessionPtr session) {
    std::string reason = args.empty() ? "Quit" : args[0];
    const std::string& user_id = session->user_id();

    // Part all channels
    std::vector<std::string> channels_to_leave;
    for (const auto& [chan_name, chan] : channels_) {
        if (chan.members.count(user_id)) {
            channels_to_leave.push_back(chan_name);
        }
    }

    for (const auto& chan : channels_to_leave) {
        part_channel(chan, user_id);
        notify_channel_part(chan, user_id, reason);
    }

    // Disconnect the session
    session->disconnect("Quit: " + reason);

    spdlog::info("{} quit ({})", user_id, reason);
    return make_response(true, "Goodbye!", "quit");
}

// ============================================================================
// /msg <nick> <message>
// ============================================================================
CommandResponse CommandHandler::cmd_msg(const std::vector<std::string>& args, SessionPtr session) {
    if (args.size() < 2) {
        return make_response(false, "Usage: /msg <nick> <message>", "msg");
    }

    std::string target = args[0];
    std::string message = args[1];
    for (size_t i = 2; i < args.size(); ++i) {
        message += " " + args[i];
    }

    // Look up target by nick or user_id
    std::string target_id;
    target_id = resolve_target_user_id(nick_to_user_id_, target);

    // Find target session
    auto target_session = find_session_(target_id);
    if (!target_session) {
        return make_response(false, target + " is offline", "msg");
    }

    // Send the message as a private message envelope
    // For now, we just notify the sender that the message was sent
    // The actual message delivery would be handled via the chat envelope flow

    spdlog::debug("Private message from {} to {}: {}", session->user_id(), target_id, message);
    return make_response(true, "-> " + target + ": " + message, "msg");
}

CommandResponse CommandHandler::cmd_away(const std::vector<std::string>& args, SessionPtr session) {
    if (!session) {
        return make_response(false, "Session unavailable", "away");
    }
    const std::string& user_id = session->user_id();
    const std::string reason = join_command_args(args);
    if (update_presence_) {
        update_presence_(user_id, PresenceUpdate::AWAY, reason, nullptr);
    }
    return make_response(true,
                         reason.empty() ? "Status set to away"
                                        : "Status set to away: " + reason,
                         "away");
}

CommandResponse CommandHandler::cmd_back(const std::vector<std::string>& args, SessionPtr session) {
    if (!args.empty()) {
        return make_response(false, "Usage: /back", "back");
    }
    if (!session) {
        return make_response(false, "Session unavailable", "back");
    }
    if (update_presence_) {
        update_presence_(session->user_id(), PresenceUpdate::ONLINE, "", nullptr);
    }
    return make_response(true, "Status set to online", "back");
}

CommandResponse CommandHandler::cmd_dnd(const std::vector<std::string>& args, SessionPtr session) {
    if (!session) {
        return make_response(false, "Session unavailable", "dnd");
    }
    const std::string reason = join_command_args(args);
    if (update_presence_) {
        update_presence_(session->user_id(), PresenceUpdate::DND, reason, nullptr);
    }
    return make_response(true,
                         reason.empty() ? "Status set to do not disturb"
                                        : "Status set to do not disturb: " + reason,
                         "dnd");
}

CommandResponse CommandHandler::cmd_quota(const std::vector<std::string>& args, SessionPtr session) {
    if (!args.empty()) {
        return make_response(false, "Usage: /quota", "quota");
    }

    const auto user_reserved = file_store_.getUserReservedBytes(session->user_id());
    const auto total_reserved = file_store_.getReservedBytes();

    std::ostringstream message;
    message << "File storage quotas:\n";
    message << "Your usage: " << human_bytes(user_reserved) << "\n";
    message << "Your limit: " << quota_limit_label(max_user_storage_bytes_) << "\n";
    if (max_user_storage_bytes_ > 0) {
        message << "Your remaining: "
                << quota_remaining_label(user_reserved, max_user_storage_bytes_) << "\n";
    }
    message << "Server usage: " << human_bytes(total_reserved) << "\n";
    message << "Server limit: " << quota_limit_label(max_total_storage_bytes_);
    if (max_total_storage_bytes_ > 0) {
        message << "\nServer remaining: "
                << quota_remaining_label(total_reserved, max_total_storage_bytes_);
    }

    return make_response(true, message.str(), "quota");
}

CommandResponse CommandHandler::cmd_rmfile(const std::vector<std::string>& args, SessionPtr session) {
    if (args.empty()) {
        return make_response(false, "Usage: /rmfile <file-id>", "rmfile");
    }

    const auto metadata = file_store_.getFile(args[0]);
    if (!metadata.has_value()) {
        return make_response(false, "File not found: " + args[0], "rmfile");
    }

    const auto& user_id = session->user_id();
    const bool sender_match = (metadata->sender_id == user_id);
    const bool channel_operator = !metadata->channel_id.empty() && is_operator(metadata->channel_id, user_id);
    if (!sender_match && !channel_operator) {
        return make_response(false, "Permission denied for file delete", "rmfile");
    }

    if (!file_store_.deleteFile(metadata->file_id)) {
        return make_response(false, "Failed to delete file: " + metadata->file_id, "rmfile");
    }

    FileChanged changed;
    changed.set_file_id(metadata->file_id);
    changed.set_recipient_id(metadata->recipient_id);
    changed.set_channel_id(metadata->channel_id);

    Envelope env;
    env.set_type(MT_FILE_CHANGED);
    changed.SerializeToString(env.mutable_payload());
    broadcast_(env, nullptr);

    return make_response(true, "Deleted file " + metadata->filename, "rmfile");
}

// ============================================================================
// Rate Limiting & Abuse Tracking
// ============================================================================

bool CommandHandler::track_abuse(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(abuse_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto [it, inserted] = abuse_records_.emplace(user_id, AbuseRecord{});
    
    AbuseRecord& record = it->second;
    
    if (inserted) {
        record.first_violation = now;
    }
    
    // Check if we should reset the violation window
    if (now - record.first_violation > kViolationWindow) {
        record.violations = 0;
        record.first_violation = now;
        record.banned = false;
    }
    
    record.violations++;
    record.last_violation = now;
    
    // Ban after max violations
    if (record.violations >= kMaxViolations) {
        record.banned = true;
        spdlog::warn("User {} auto-banned for {} rate limit violations within {} minutes",
            user_id, record.violations, 
            std::chrono::duration_cast<std::chrono::minutes>(kViolationWindow).count());
        return true;  // User is now banned
    }
    
    return false;  // Not banned yet
}

bool CommandHandler::is_abuser(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(abuse_mutex_);
    
    auto it = abuse_records_.find(user_id);
    if (it == abuse_records_.end()) {
        return false;
    }
    
    const AbuseRecord& record = it->second;
    
    // Check if ban has expired
    if (record.banned) {
        auto now = std::chrono::steady_clock::now();
        if (now - record.last_violation > kBanDuration) {
            // Ban expired, remove the record
            abuse_records_.erase(it);
            spdlog::info("Ban expired for user {}", user_id);
            return false;
        }
        return true;  // Still banned
    }
    
    return false;
}

void CommandHandler::cleanup_old_abuse_records() {
    std::lock_guard<std::mutex> lock(abuse_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = abuse_records_.begin(); it != abuse_records_.end();) {
        const AbuseRecord& record = it->second;
        
        // Remove records where ban has expired OR violations are old and not banned
        bool should_remove = false;
        
        if (record.banned) {
            // Remove if ban has expired
            should_remove = (now - record.last_violation > kBanDuration);
        } else {
            // Remove if violations are outside the window
            should_remove = (now - record.first_violation > kViolationWindow);
        }
        
        if (should_remove) {
            it = abuse_records_.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// /resetdb - Admin command to clear all user data (for testing)
// ============================================================================
CommandResponse CommandHandler::cmd_resetdb(const std::vector<std::string>& args, SessionPtr session) {
    (void)args; // Unused
    
    const std::string& user_id = session->user_id();
    
    // Only allow the first user (server owner) or a hardcoded admin to reset
    // In production, you'd want proper admin roles
    static const std::string admin_user = "admin";  // Change this to your admin username
    
    if (user_id != admin_user) {
        spdlog::warn("User {} attempted to reset database without permission", user_id);
        return make_response(false, "Permission denied: only admin can reset database", "resetdb");
    }
    
    try {
        std::lock_guard<std::mutex> lock(db_.mutex());
        
        // Clear all tables
        db_.get().exec("DELETE FROM users");
        db_.get().exec("DELETE FROM signed_prekeys");
        db_.get().exec("DELETE FROM one_time_prekeys");
        db_.get().exec("DELETE FROM offline_messages");
        
        spdlog::warn("Database reset by admin user: {}", user_id);
        return make_response(true, "Database cleared successfully. All users and keys removed.", "resetdb");
    } catch (const SQLite::Exception& e) {
        spdlog::error("Failed to reset database: {}", e.what());
        return make_response(false, "Database error: " + std::string(e.what()), "resetdb");
    }
}

} // namespace grotto::commands
