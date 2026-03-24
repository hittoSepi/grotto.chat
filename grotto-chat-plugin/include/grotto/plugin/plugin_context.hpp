#pragma once

#include <functional>
#include <string>

namespace spdlog { class logger; }

namespace grotto::plugin {

// Callback types for host integration.
// The host application sets these to wire plugin actions to real functionality.
using SendMessageFn    = std::function<void(const std::string& channel, const std::string& text)>;
using JoinChannelFn    = std::function<void(const std::string& channel)>;
using LeaveChannelFn   = std::function<void(const std::string& channel)>;
using KickUserFn       = std::function<void(const std::string& user_id, const std::string& reason)>;
using BanUserFn        = std::function<void(const std::string& user_id, const std::string& reason)>;
using CreateChannelFn  = std::function<void(const std::string& channel_id)>;
using DeleteChannelFn  = std::function<void(const std::string& channel_id)>;
using GetOnlineUsersFn = std::function<std::vector<std::string>()>;
using GetChannelsFn    = std::function<std::vector<std::string>()>;
using GetActiveChFn    = std::function<std::string()>;
using NotifyFn         = std::function<void(const std::string& text)>;

// Shared context passed to all plugins.
// The host application populates the callbacks it supports.
struct PluginContext {
    std::shared_ptr<spdlog::logger> logger;

    // Bot callbacks
    SendMessageFn   send_message;
    JoinChannelFn   join_channel;
    LeaveChannelFn  leave_channel;

    // Server extension callbacks
    KickUserFn       kick_user;
    BanUserFn        ban_user;
    CreateChannelFn  create_channel;
    DeleteChannelFn  delete_channel;
    GetOnlineUsersFn get_online_users;

    // Client extension callbacks
    GetChannelsFn    get_channels;
    GetActiveChFn    get_active_channel;
    NotifyFn         notify;

    // Bug report callback (client → server)
    using SendBugReportFn = std::function<void(const std::string& description)>;
    SendBugReportFn send_bug_report;

    // Bug report handler (server receives from client)
    using OnBugReportFn = std::function<void(const std::string& user_id, const std::string& description)>;
    OnBugReportFn on_bug_report;
};

} // namespace grotto::plugin
