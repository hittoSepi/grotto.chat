#include "grotto/plugin/bot_plugin_instance.hpp"

#include <spdlog/spdlog.h>

extern "C" {
#include "quickjs.h"
}

namespace grotto::plugin {

// ── JS bindings for grotto.bot.* ────────────────────────────

static PluginInstance* get_plugin(JSContext* ctx) {
    return static_cast<PluginInstance*>(JS_GetContextOpaque(ctx));
}

static JSValue js_bot_join_channel(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "joinChannel() requires 1 argument");

    const char* channel = JS_ToCString(ctx, argv[0]);
    if (!channel) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    if (plugin->ctx().join_channel) {
        plugin->ctx().join_channel(channel);
    }

    JS_FreeCString(ctx, channel);
    return JS_UNDEFINED;
}

static JSValue js_bot_leave_channel(JSContext* ctx, JSValueConst /*this_val*/,
                                     int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "leaveChannel() requires 1 argument");

    const char* channel = JS_ToCString(ctx, argv[0]);
    if (!channel) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    if (plugin->ctx().leave_channel) {
        plugin->ctx().leave_channel(channel);
    }

    JS_FreeCString(ctx, channel);
    return JS_UNDEFINED;
}

// ── BotPluginInstance ───────────────────────────────────────

BotPluginInstance::BotPluginInstance(PluginMeta meta, PluginContext& ctx)
    : PluginInstance(std::move(meta), ctx) {}

BotPluginInstance::~BotPluginInstance() = default;

void BotPluginInstance::shutdown() {
    PluginInstance::shutdown();
}

void BotPluginInstance::setup_type_specific_api() {
    JSValue global = JS_GetGlobalObject(js_);
    JSValue grotto = JS_GetPropertyStr(js_, global, "grotto");

    // grotto.bot namespace
    JSValue bot = JS_NewObject(js_);

    // grotto.bot.userId
    if (meta_.bot) {
        JS_SetPropertyStr(js_, bot, "userId",
            JS_NewString(js_, meta_.bot->user_id.c_str()));
    }

    JS_SetPropertyStr(js_, bot, "joinChannel",
        JS_NewCFunction(js_, js_bot_join_channel, "joinChannel", 1));
    JS_SetPropertyStr(js_, bot, "leaveChannel",
        JS_NewCFunction(js_, js_bot_leave_channel, "leaveChannel", 1));

    JS_SetPropertyStr(js_, grotto, "bot", bot);

    JS_FreeValue(js_, grotto);
    JS_FreeValue(js_, global);
}

} // namespace grotto::plugin
