#include "grotto/plugin/client_ext_plugin.hpp"

#include <spdlog/spdlog.h>

extern "C" {
#include "quickjs.h"
}

namespace grotto::plugin {

static PluginInstance* get_plugin(JSContext* ctx) {
    return static_cast<PluginInstance*>(JS_GetContextOpaque(ctx));
}

static JSValue js_client_active_channel(JSContext* ctx, JSValueConst,
                                         int, JSValueConst*) {
    auto* plugin = get_plugin(ctx);
    if (plugin->ctx().get_active_channel) {
        auto ch = plugin->ctx().get_active_channel();
        return JS_NewString(ctx, ch.c_str());
    }
    return JS_NewString(ctx, "");
}

static JSValue js_client_channels(JSContext* ctx, JSValueConst,
                                   int, JSValueConst*) {
    auto* plugin = get_plugin(ctx);
    JSValue arr = JS_NewArray(ctx);
    if (plugin->ctx().get_channels) {
        auto channels = plugin->ctx().get_channels();
        for (uint32_t i = 0; i < channels.size(); i++) {
            JS_SetPropertyUint32(ctx, arr, i,
                JS_NewString(ctx, channels[i].c_str()));
        }
    }
    return arr;
}

static JSValue js_client_online_users(JSContext* ctx, JSValueConst,
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

static JSValue js_client_notify(JSContext* ctx, JSValueConst,
                                 int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "notify() requires 1 argument");

    const char* text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    if (plugin->ctx().notify) {
        plugin->ctx().notify(text);
    }

    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_client_send_bug_report(JSContext* ctx, JSValueConst,
                                          int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "sendBugReport() requires 1 argument");

    const char* description = JS_ToCString(ctx, argv[0]);
    if (!description) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    if (plugin->ctx().send_bug_report) {
        plugin->ctx().send_bug_report(description);
    }

    JS_FreeCString(ctx, description);
    return JS_UNDEFINED;
}

ClientExtPlugin::ClientExtPlugin(PluginMeta meta, PluginContext& ctx)
    : PluginInstance(std::move(meta), ctx) {}

ClientExtPlugin::~ClientExtPlugin() = default;

void ClientExtPlugin::shutdown() {
    PluginInstance::shutdown();
}

void ClientExtPlugin::setup_type_specific_api() {
    JSValue global = JS_GetGlobalObject(js_);
    JSValue grotto = JS_GetPropertyStr(js_, global, "grotto");

    JSValue client = JS_NewObject(js_);

    JS_SetPropertyStr(js_, client, "activeChannel",
        JS_NewCFunction(js_, js_client_active_channel, "activeChannel", 0));
    JS_SetPropertyStr(js_, client, "channels",
        JS_NewCFunction(js_, js_client_channels, "channels", 0));
    JS_SetPropertyStr(js_, client, "onlineUsers",
        JS_NewCFunction(js_, js_client_online_users, "onlineUsers", 0));
    JS_SetPropertyStr(js_, client, "notify",
        JS_NewCFunction(js_, js_client_notify, "notify", 1));
    JS_SetPropertyStr(js_, client, "sendBugReport",
        JS_NewCFunction(js_, js_client_send_bug_report, "sendBugReport", 1));

    JS_SetPropertyStr(js_, grotto, "client", client);

    JS_FreeValue(js_, grotto);
    JS_FreeValue(js_, global);
}

} // namespace grotto::plugin
