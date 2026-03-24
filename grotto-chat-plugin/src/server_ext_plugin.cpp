#include "grotto/plugin/server_ext_plugin.hpp"

#include <spdlog/spdlog.h>

extern "C" {
#include "quickjs.h"
}

namespace grotto::plugin {

static PluginInstance* get_plugin(JSContext* ctx) {
    return static_cast<PluginInstance*>(JS_GetContextOpaque(ctx));
}

// Bug report handler storage
static JSValue bug_report_handler = JS_NULL;

void call_js_bug_report_handler(JSContext* ctx, const std::string& user_id, const std::string& description) {
    if (JS_IsNull(bug_report_handler)) return;

    JSValue args[2];
    args[0] = JS_NewString(ctx, user_id.c_str());
    args[1] = JS_NewString(ctx, description.c_str());

    JSValue result = JS_Call(ctx, bug_report_handler, JS_UNDEFINED, 2, args);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exc);
        spdlog::error("[PLUGIN] Bug report handler exception: {}", str ? str : "<unknown>");
        if (str) JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, exc);
    }

    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, result);
}

static JSValue js_server_kick_user(JSContext* ctx, JSValueConst,
                                    int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "kickUser() requires 2 arguments");

    const char* user_id = JS_ToCString(ctx, argv[0]);
    const char* reason = JS_ToCString(ctx, argv[1]);
    if (!user_id || !reason) {
        if (user_id) JS_FreeCString(ctx, user_id);
        if (reason) JS_FreeCString(ctx, reason);
        return JS_EXCEPTION;
    }

    auto* plugin = get_plugin(ctx);
    if (plugin->meta().has_permission(Permission::ManageUsers) &&
        plugin->ctx().kick_user) {
        plugin->ctx().kick_user(user_id, reason);
    }

    JS_FreeCString(ctx, user_id);
    JS_FreeCString(ctx, reason);
    return JS_UNDEFINED;
}

static JSValue js_server_ban_user(JSContext* ctx, JSValueConst,
                                   int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "banUser() requires 2 arguments");

    const char* user_id = JS_ToCString(ctx, argv[0]);
    const char* reason = JS_ToCString(ctx, argv[1]);
    if (!user_id || !reason) {
        if (user_id) JS_FreeCString(ctx, user_id);
        if (reason) JS_FreeCString(ctx, reason);
        return JS_EXCEPTION;
    }

    auto* plugin = get_plugin(ctx);
    if (plugin->meta().has_permission(Permission::ManageUsers) &&
        plugin->ctx().ban_user) {
        plugin->ctx().ban_user(user_id, reason);
    }

    JS_FreeCString(ctx, user_id);
    JS_FreeCString(ctx, reason);
    return JS_UNDEFINED;
}

static JSValue js_server_create_channel(JSContext* ctx, JSValueConst,
                                         int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "createChannel() requires 1 argument");

    const char* channel_id = JS_ToCString(ctx, argv[0]);
    if (!channel_id) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    if (plugin->meta().has_permission(Permission::ManageChannels) &&
        plugin->ctx().create_channel) {
        plugin->ctx().create_channel(channel_id);
    }

    JS_FreeCString(ctx, channel_id);
    return JS_UNDEFINED;
}

static JSValue js_server_delete_channel(JSContext* ctx, JSValueConst,
                                         int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "deleteChannel() requires 1 argument");

    const char* channel_id = JS_ToCString(ctx, argv[0]);
    if (!channel_id) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    if (plugin->meta().has_permission(Permission::ManageChannels) &&
        plugin->ctx().delete_channel) {
        plugin->ctx().delete_channel(channel_id);
    }

    JS_FreeCString(ctx, channel_id);
    return JS_UNDEFINED;
}

static JSValue js_server_online_users(JSContext* ctx, JSValueConst,
                                       int, JSValueConst*) {
    auto* plugin = get_plugin(ctx);
    JSValue arr = JS_NewArray(ctx);
    if (plugin->ctx().get_online_users) {
        auto users = plugin->ctx().get_online_users();
        for (uint32_t i = 0; i < users.size(); i++) {
            JS_SetPropertyUint32(ctx, arr, i,
                JS_NewString(ctx, users[i].c_str()));
        }
    }
    return arr;
}

static JSValue js_server_on_bug_report(JSContext* ctx, JSValueConst,
                                        int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "onBugReport() requires 1 argument");

    if (!JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "onBugReport() argument must be a function");
    }

    // Free old handler if exists
    if (!JS_IsNull(bug_report_handler)) {
        JS_FreeValue(ctx, bug_report_handler);
    }

    // Store new handler
    bug_report_handler = JS_DupValue(ctx, argv[0]);

    // Register with host
    auto* plugin = get_plugin(ctx);
    if (plugin->ctx().on_bug_report) {
        plugin->ctx().on_bug_report = [&plugin, ctx](const std::string& user, const std::string& desc) {
            call_js_bug_report_handler(ctx, user, desc);
        };
    }

    return JS_UNDEFINED;
}

ServerExtPlugin::ServerExtPlugin(PluginMeta meta, PluginContext& ctx)
    : PluginInstance(std::move(meta), ctx) {}

ServerExtPlugin::~ServerExtPlugin() = default;

void ServerExtPlugin::shutdown() {
    PluginInstance::shutdown();
}

void ServerExtPlugin::on_event(const PluginEvent& event) {
    // Server extensions never see plaintext message content
    if (event.type == PluginEvent::Type::MessageReceived) return;

    PluginInstance::on_event(event);
}

void ServerExtPlugin::setup_type_specific_api() {
    JSValue global = JS_GetGlobalObject(js_);
    JSValue grotto = JS_GetPropertyStr(js_, global, "grotto");

    JSValue server = JS_NewObject(js_);

    JS_SetPropertyStr(js_, server, "kickUser",
        JS_NewCFunction(js_, js_server_kick_user, "kickUser", 2));
    JS_SetPropertyStr(js_, server, "banUser",
        JS_NewCFunction(js_, js_server_ban_user, "banUser", 2));
    JS_SetPropertyStr(js_, server, "createChannel",
        JS_NewCFunction(js_, js_server_create_channel, "createChannel", 1));
    JS_SetPropertyStr(js_, server, "deleteChannel",
        JS_NewCFunction(js_, js_server_delete_channel, "deleteChannel", 1));
    JS_SetPropertyStr(js_, server, "onlineUsers",
        JS_NewCFunction(js_, js_server_online_users, "onlineUsers", 0));
    JS_SetPropertyStr(js_, server, "onBugReport",
        JS_NewCFunction(js_, js_server_on_bug_report, "onBugReport", 1));

    JS_SetPropertyStr(js_, grotto, "server", server);

    JS_FreeValue(js_, grotto);
    JS_FreeValue(js_, global);
}

} // namespace grotto::plugin
