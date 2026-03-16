#include "grotto/plugin/plugin_instance.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>

namespace grotto::plugin {

// ── Error code support ──────────────────────────────────────

namespace {

class PluginErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "grotto-plugin"; }
    std::string message(int ev) const override {
        switch (static_cast<PluginError>(ev)) {
            case PluginError::None: return "success";
            case PluginError::RuntimeInitFailed: return "QuickJS runtime init failed";
            case PluginError::ContextInitFailed: return "QuickJS context init failed";
            case PluginError::SourceFileNotFound: return "plugin source file not found";
            case PluginError::ScriptInitFailed: return "plugin script init failed";
            case PluginError::InvalidMeta: return "invalid plugin metadata";
            case PluginError::AlreadyLoaded: return "plugin already loaded";
            case PluginError::NotFound: return "plugin not found";
        }
        return "unknown error";
    }
};

const PluginErrorCategory& plugin_error_category() {
    static PluginErrorCategory cat;
    return cat;
}

} // anonymous namespace

std::error_code make_error_code(PluginError e) {
    return {static_cast<int>(e), plugin_error_category()};
}

// ── PluginInstance ───────────────────────────────────────────

PluginInstance::PluginInstance(PluginMeta meta, PluginContext& ctx)
    : meta_(std::move(meta)), ctx_(ctx) {}

PluginInstance::~PluginInstance() {
    if (state_ == PluginState::Running) {
        shutdown();
    }
    // Free all stored JS handlers
    if (js_) {
        for (auto& [name, handlers] : event_handlers_) {
            for (auto& fn : handlers) {
                JS_FreeValue(js_, fn);
            }
        }
        for (auto& [name, fn] : command_handlers_) {
            JS_FreeValue(js_, fn);
        }
    }
    if (js_) JS_FreeContext(js_);
    if (rt_) JS_FreeRuntime(rt_);
}

std::error_code PluginInstance::init() {
    // 1. Create runtime
    rt_ = JS_NewRuntime();
    if (!rt_) return PluginError::RuntimeInitFailed;

    JS_SetMemoryLimit(rt_, 16 * 1024 * 1024); // 16 MB per plugin
    JS_SetMaxStackSize(rt_, 1024 * 1024);      // 1 MB stack

    // 2. Create context
    js_ = JS_NewContext(rt_);
    if (!js_) return PluginError::ContextInitFailed;

    // Store 'this' as opaque pointer for C callbacks
    JS_SetContextOpaque(js_, this);

    // 3. Setup API
    setup_js_api();
    setup_type_specific_api();

    // 4. Load and execute entry point script
    auto script_path = meta_.plugin_dir / meta_.entry;
    std::ifstream f(script_path);
    if (!f.is_open()) {
        spdlog::error("[PLUGIN:{}] Cannot open entry file: {}",
                      meta_.name, script_path.string());
        return PluginError::SourceFileNotFound;
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string script = ss.str();

    JSValue result = JS_Eval(js_, script.c_str(), script.size(),
                              meta_.entry.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(js_);
        const char* str = JS_ToCString(js_, exc);
        spdlog::error("[PLUGIN:{}] Script init error: {}",
                      meta_.name, str ? str : "<unknown>");
        if (str) JS_FreeCString(js_, str);
        JS_FreeValue(js_, exc);
        JS_FreeValue(js_, result);
        return PluginError::ScriptInitFailed;
    }
    JS_FreeValue(js_, result);

    state_ = PluginState::Running;
    spdlog::info("[PLUGIN:{}] Loaded successfully (type: {})",
                 meta_.name, plugin_type_to_string(meta_.type));
    return PluginError::None;
}

void PluginInstance::shutdown() {
    if (state_ != PluginState::Running) return;

    // Call shutdown handlers if registered
    auto it = event_handlers_.find("shutdown");
    if (it != event_handlers_.end()) {
        for (auto& fn : it->second) {
            call_js_safe(fn, nullptr, 0);
        }
    }

    state_ = PluginState::Stopped;
    spdlog::info("[PLUGIN:{}] Shut down", meta_.name);
}

void PluginInstance::on_event(const PluginEvent& event) {
    if (state_ != PluginState::Running) return;

    auto event_name = event_type_to_string(event.type);
    auto it = event_handlers_.find(event_name);
    if (it == event_handlers_.end()) return;

    // Build JS event object
    JSValue js_event = JS_NewObject(js_);
    JS_SetPropertyStr(js_, js_event, "type",
        JS_NewString(js_, event_name));
    JS_SetPropertyStr(js_, js_event, "channel",
        JS_NewString(js_, event.channel.c_str()));
    JS_SetPropertyStr(js_, js_event, "sender",
        JS_NewString(js_, event.user_id.c_str()));
    JS_SetPropertyStr(js_, js_event, "text",
        JS_NewString(js_, event.text.c_str()));
    JS_SetPropertyStr(js_, js_event, "timestamp",
        JS_NewFloat64(js_, static_cast<double>(event.timestamp_ms)));

    // Dispatch to all registered handlers
    for (auto& handler : it->second) {
        JSValue args[] = { js_event };
        call_js_safe(handler, args, 1);
    }

    JS_FreeValue(js_, js_event);
}

void PluginInstance::on_command(const PluginCommand& cmd) {
    if (state_ != PluginState::Running) return;

    auto it = command_handlers_.find(cmd.name);
    if (it == command_handlers_.end()) return;

    // Build command context object
    JSValue js_ctx = JS_NewObject(js_);
    JS_SetPropertyStr(js_, js_ctx, "command",
        JS_NewString(js_, cmd.name.c_str()));
    JS_SetPropertyStr(js_, js_ctx, "channel",
        JS_NewString(js_, cmd.channel.c_str()));
    JS_SetPropertyStr(js_, js_ctx, "sender",
        JS_NewString(js_, cmd.sender_id.c_str()));

    // Build args array
    JSValue js_args = JS_NewArray(js_);
    for (size_t i = 0; i < cmd.args.size(); i++) {
        JS_SetPropertyUint32(js_, js_args, static_cast<uint32_t>(i),
            JS_NewString(js_, cmd.args[i].c_str()));
    }
    JS_SetPropertyStr(js_, js_ctx, "args", js_args);

    JSValue args[] = { js_ctx };
    call_js_safe(it->second, args, 1);

    JS_FreeValue(js_, js_ctx);
}

void PluginInstance::tick() {
    if (state_ != PluginState::Running || !rt_) return;

    // Execute pending JS microtasks
    JSContext* ctx;
    while (JS_ExecutePendingJob(rt_, &ctx) > 0) {}

    // Check if plugin entered error state
    if (error_count_ >= kMaxErrorsBeforeUnload) {
        spdlog::error("[PLUGIN:{}] Too many errors ({}), marking as Error",
                      meta_.name, error_count_);
        state_ = PluginState::Error;
    }
}

PluginInfo PluginInstance::info() const {
    return {meta_.name, meta_.display_name, meta_.version, meta_.type, state_};
}

void PluginInstance::register_event_handler(const std::string& event_name, JSValue fn) {
    event_handlers_[event_name].push_back(JS_DupValue(js_, fn));
}

void PluginInstance::register_command_handler(const std::string& cmd_name, JSValue fn) {
    auto it = command_handlers_.find(cmd_name);
    if (it != command_handlers_.end()) {
        JS_FreeValue(js_, it->second);
    }
    command_handlers_[cmd_name] = JS_DupValue(js_, fn);
}

std::vector<CommandInfo> PluginInstance::registered_commands() const {
    std::vector<CommandInfo> result;
    for (auto& [name, _fn] : command_handlers_) {
        // Find matching CommandDef for description/usage
        CommandInfo ci;
        ci.name = name;
        ci.plugin_name = meta_.name;
        for (auto& cd : meta_.commands) {
            if (cd.name == name) {
                ci.description = cd.description;
                ci.usage = cd.usage;
                break;
            }
        }
        result.push_back(std::move(ci));
    }
    return result;
}

bool PluginInstance::call_js_safe(JSValue fn, JSValue* args, int argc) {
    JSValue result = JS_Call(js_, fn, JS_UNDEFINED, argc, args);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(js_);
        const char* str = JS_ToCString(js_, exc);
        spdlog::error("[PLUGIN:{}] JS exception: {}",
                      meta_.name, str ? str : "<unknown>");
        if (str) JS_FreeCString(js_, str);
        JS_FreeValue(js_, exc);
        JS_FreeValue(js_, result);
        error_count_++;
        return false;
    }

    JS_FreeValue(js_, result);
    return true;
}

} // namespace grotto::plugin
