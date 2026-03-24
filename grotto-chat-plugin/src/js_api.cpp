#include "grotto/plugin/plugin_instance.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

extern "C" {
#include "quickjs.h"
}

namespace grotto::plugin {

// ── Helper: get PluginInstance* from JSContext ───────────────

static PluginInstance* get_plugin(JSContext* ctx) {
    return static_cast<PluginInstance*>(JS_GetContextOpaque(ctx));
}

// ── grotto.on(event_name, callback) ─────────────────────────

static JSValue js_grotto_on(JSContext* ctx, JSValueConst /*this_val*/,
                             int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "on() requires 2 arguments");

    const char* event_name = JS_ToCString(ctx, argv[0]);
    if (!event_name) return JS_EXCEPTION;

    if (!JS_IsFunction(ctx, argv[1])) {
        JS_FreeCString(ctx, event_name);
        return JS_ThrowTypeError(ctx, "on() second argument must be a function");
    }

    auto* plugin = get_plugin(ctx);
    plugin->register_event_handler(event_name, argv[1]);

    JS_FreeCString(ctx, event_name);
    return JS_UNDEFINED;
}

// ── grotto.onCommand(name, options, callback) ───────────────

static JSValue js_grotto_on_command(JSContext* ctx, JSValueConst /*this_val*/,
                                     int argc, JSValueConst* argv) {
    if (argc < 3) return JS_ThrowTypeError(ctx, "onCommand() requires 3 arguments");

    const char* cmd_name = JS_ToCString(ctx, argv[0]);
    if (!cmd_name) return JS_EXCEPTION;

    // argv[1] = options object (we ignore it here, metadata comes from plugin.json)
    // argv[2] = callback function

    if (!JS_IsFunction(ctx, argv[2])) {
        JS_FreeCString(ctx, cmd_name);
        return JS_ThrowTypeError(ctx, "onCommand() third argument must be a function");
    }

    auto* plugin = get_plugin(ctx);
    plugin->register_command_handler(cmd_name, argv[2]);

    JS_FreeCString(ctx, cmd_name);
    return JS_UNDEFINED;
}

// ── grotto.sendMessage(channel, text) ───────────────────────

static JSValue js_grotto_send_message(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "sendMessage() requires 2 arguments");

    const char* channel = JS_ToCString(ctx, argv[0]);
    const char* text = JS_ToCString(ctx, argv[1]);

    if (!channel || !text) {
        if (channel) JS_FreeCString(ctx, channel);
        if (text) JS_FreeCString(ctx, text);
        return JS_EXCEPTION;
    }

    auto* plugin = get_plugin(ctx);
    if (plugin->meta().has_permission(Permission::SendMessages) &&
        plugin->ctx().send_message) {
        plugin->ctx().send_message(channel, text);
    } else {
        spdlog::warn("[PLUGIN:{}] sendMessage denied (missing permission or callback)",
                     plugin->meta().name);
    }

    JS_FreeCString(ctx, channel);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

// ── grotto.log.{info,warn,error,debug} ──────────────────────

static JSValue js_log_level(JSContext* ctx, int argc, JSValueConst* argv,
                             spdlog::level::level_enum level) {
    if (argc < 1) return JS_UNDEFINED;

    const char* msg = JS_ToCString(ctx, argv[0]);
    if (!msg) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    spdlog::log(level, "[PLUGIN:{}] {}", plugin->meta().name, msg);
    JS_FreeCString(ctx, msg);
    return JS_UNDEFINED;
}

static JSValue js_log_info(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    return js_log_level(ctx, argc, argv, spdlog::level::info);
}
static JSValue js_log_warn(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    return js_log_level(ctx, argc, argv, spdlog::level::warn);
}
static JSValue js_log_error(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    return js_log_level(ctx, argc, argv, spdlog::level::err);
}
static JSValue js_log_debug(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    return js_log_level(ctx, argc, argv, spdlog::level::debug);
}

// ── grotto.getConfig() ──────────────────────────────────────

static JSValue js_grotto_get_config(JSContext* ctx, JSValueConst /*this_val*/,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    // TODO: parse config.toml and return as JS object
    return JS_NewObject(ctx);
}

// ── grotto.getPluginDir() ───────────────────────────────────

static JSValue js_grotto_get_plugin_dir(JSContext* ctx, JSValueConst /*this_val*/,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    auto* plugin = get_plugin(ctx);
    return JS_NewString(ctx, plugin->meta().plugin_dir.string().c_str());
}

// ── grotto.getDataDir() ─────────────────────────────────────

static JSValue js_grotto_get_data_dir(JSContext* ctx, JSValueConst /*this_val*/,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    auto* plugin = get_plugin(ctx);
    auto data_dir = plugin->meta().plugin_dir / "data";
    return JS_NewString(ctx, data_dir.string().c_str());
}

// ── console.log (convenience) ───────────────────────────────

static JSValue js_console_log(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    return js_log_level(ctx, argc, argv, spdlog::level::info);
}

// ── grotto.fs.readFile(path) ─────────────────────────────────

static JSValue js_fs_read_file(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "readFile() requires 1 argument");

    const char* rel_path = JS_ToCString(ctx, argv[0]);
    if (!rel_path) return JS_EXCEPTION;

    auto* plugin = get_plugin(ctx);
    if (!plugin->meta().has_permission(Permission::FileRead)) {
        JS_FreeCString(ctx, rel_path);
        return JS_ThrowTypeError(ctx, "Missing file_read permission");
    }

    // Sanitize path: must stay within plugin's data/ directory
    std::filesystem::path data_dir = plugin->meta().plugin_dir / "data";
    std::filesystem::path target_path = data_dir / rel_path;

    // Resolve and check for path traversal
    std::error_code ec;
    auto canonical_data = std::filesystem::canonical(data_dir, ec);
    auto canonical_target = std::filesystem::weakly_canonical(target_path, ec);

    if (ec || !canonical_target.string().starts_with(canonical_data.string())) {
        JS_FreeCString(ctx, rel_path);
        return JS_ThrowTypeError(ctx, "Invalid path (escapes data directory)");
    }

    std::ifstream f(canonical_target, std::ios::binary);
    if (!f.is_open()) {
        JS_FreeCString(ctx, rel_path);
        return JS_ThrowTypeError(ctx, "Failed to open file for reading");
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    JS_FreeCString(ctx, rel_path);
    return JS_NewString(ctx, content.c_str());
}

// ── grotto.fs.writeFile(path, content) ────────────────────────

static JSValue js_fs_write_file(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "writeFile() requires 2 arguments");

    const char* rel_path = JS_ToCString(ctx, argv[0]);
    const char* content = JS_ToCString(ctx, argv[1]);

    if (!rel_path || !content) {
        if (rel_path) JS_FreeCString(ctx, rel_path);
        if (content) JS_FreeCString(ctx, content);
        return JS_EXCEPTION;
    }

    auto* plugin = get_plugin(ctx);
    if (!plugin->meta().has_permission(Permission::FileWrite)) {
        JS_FreeCString(ctx, rel_path);
        JS_FreeCString(ctx, content);
        return JS_ThrowTypeError(ctx, "Missing file_write permission");
    }

    // Sanitize path: must stay within plugin's data/ directory
    std::filesystem::path data_dir = plugin->meta().plugin_dir / "data";
    std::filesystem::path target_path = data_dir / rel_path;

    // Create data directory if needed
    std::filesystem::create_directories(data_dir);

    // Resolve and check for path traversal
    std::error_code ec;
    auto canonical_data = std::filesystem::canonical(data_dir, ec);
    if (ec) {
        // Directory didn't exist, try weakly_canonical
        canonical_data = std::filesystem::weakly_canonical(data_dir, ec);
    }
    auto canonical_target = std::filesystem::weakly_canonical(target_path, ec);

    if (!canonical_target.string().starts_with(canonical_data.string())) {
        JS_FreeCString(ctx, rel_path);
        JS_FreeCString(ctx, content);
        return JS_ThrowTypeError(ctx, "Invalid path (escapes data directory)");
    }

    std::ofstream f(canonical_target, std::ios::binary);
    if (!f.is_open()) {
        JS_FreeCString(ctx, rel_path);
        JS_FreeCString(ctx, content);
        return JS_ThrowTypeError(ctx, "Failed to open file for writing");
    }

    f << content;

    JS_FreeCString(ctx, rel_path);
    JS_FreeCString(ctx, content);
    return JS_UNDEFINED;
}

// ── Setup grotto global object ──────────────────────────────

void PluginInstance::setup_js_api() {
    JSValue global = JS_GetGlobalObject(js_);

    // ── grotto namespace ────────────────────────
    JSValue grotto = JS_NewObject(js_);

    JS_SetPropertyStr(js_, grotto, "on",
        JS_NewCFunction(js_, js_grotto_on, "on", 2));
    JS_SetPropertyStr(js_, grotto, "onCommand",
        JS_NewCFunction(js_, js_grotto_on_command, "onCommand", 3));
    JS_SetPropertyStr(js_, grotto, "sendMessage",
        JS_NewCFunction(js_, js_grotto_send_message, "sendMessage", 2));
    JS_SetPropertyStr(js_, grotto, "getConfig",
        JS_NewCFunction(js_, js_grotto_get_config, "getConfig", 0));
    JS_SetPropertyStr(js_, grotto, "getPluginDir",
        JS_NewCFunction(js_, js_grotto_get_plugin_dir, "getPluginDir", 0));
    JS_SetPropertyStr(js_, grotto, "getDataDir",
        JS_NewCFunction(js_, js_grotto_get_data_dir, "getDataDir", 0));

    // ── grotto.log ──────────────────────────────
    JSValue log_obj = JS_NewObject(js_);
    JS_SetPropertyStr(js_, log_obj, "info",
        JS_NewCFunction(js_, js_log_info, "info", 1));
    JS_SetPropertyStr(js_, log_obj, "warn",
        JS_NewCFunction(js_, js_log_warn, "warn", 1));
    JS_SetPropertyStr(js_, log_obj, "error",
        JS_NewCFunction(js_, js_log_error, "error", 1));
    JS_SetPropertyStr(js_, log_obj, "debug",
        JS_NewCFunction(js_, js_log_debug, "debug", 1));
    JS_SetPropertyStr(js_, grotto, "log", log_obj);

    // ── grotto.fs ───────────────────────────────
    JSValue fs_obj = JS_NewObject(js_);
    JS_SetPropertyStr(js_, fs_obj, "readFile",
        JS_NewCFunction(js_, js_fs_read_file, "readFile", 1));
    JS_SetPropertyStr(js_, fs_obj, "writeFile",
        JS_NewCFunction(js_, js_fs_write_file, "writeFile", 2));
    JS_SetPropertyStr(js_, grotto, "fs", fs_obj);

    JS_SetPropertyStr(js_, global, "grotto", grotto);

    // ── console.log (maps to grotto.log.info) ───
    JSValue console = JS_NewObject(js_);
    JS_SetPropertyStr(js_, console, "log",
        JS_NewCFunction(js_, js_console_log, "log", 1));
    JS_SetPropertyStr(js_, global, "console", console);

    JS_FreeValue(js_, global);
}

} // namespace grotto::plugin
