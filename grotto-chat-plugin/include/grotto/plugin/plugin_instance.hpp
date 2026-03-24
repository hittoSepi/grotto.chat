#pragma once

#include "grotto/plugin/plugin_meta.hpp"
#include "grotto/plugin/plugin_event.hpp"
#include "grotto/plugin/plugin_command.hpp"
#include "grotto/plugin/plugin_context.hpp"

extern "C" {
#include "quickjs.h"
}

#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace grotto::plugin {

enum class PluginState {
    Loading,
    Running,
    Error,
    Stopped,
};

enum class PluginError {
    None = 0,
    RuntimeInitFailed,
    ContextInitFailed,
    SourceFileNotFound,
    ScriptInitFailed,
    InvalidMeta,
    AlreadyLoaded,
    NotFound,
};

std::error_code make_error_code(PluginError e);

struct PluginInfo {
    std::string name;
    std::string display_name;
    std::string version;
    PluginType type;
    PluginState state;
};

struct CommandInfo {
    std::string name;
    std::string description;
    std::string usage;
    std::string plugin_name;
};

class PluginInstance {
public:
    PluginInstance(PluginMeta meta, PluginContext& ctx);
    virtual ~PluginInstance();

    PluginInstance(const PluginInstance&) = delete;
    PluginInstance& operator=(const PluginInstance&) = delete;

    // Lifecycle
    std::error_code init();
    virtual void shutdown();

    // Events
    virtual void on_event(const PluginEvent& event);
    virtual void on_command(const PluginCommand& cmd);

    // QuickJS tick
    void tick();

    // Getters
    const PluginMeta& meta() const { return meta_; }
    PluginState state() const { return state_; }
    PluginInfo info() const;

    // Registration (called from JS bindings)
    void register_event_handler(const std::string& event_name, JSValue fn);
    void register_command_handler(const std::string& cmd_name, JSValue fn);
    std::vector<CommandInfo> registered_commands() const;

    JSContext* js_context() const { return js_; }
    PluginContext& ctx() { return ctx_; }

protected:
    virtual void setup_type_specific_api() = 0;

    void setup_js_api();
    bool call_js_safe(JSValue fn, JSValue* args, int argc);

    PluginMeta    meta_;
    PluginContext& ctx_;
    PluginState   state_ = PluginState::Loading;

    JSRuntime*    rt_  = nullptr;
    JSContext*    js_  = nullptr;

    std::unordered_map<std::string, std::vector<JSValue>> event_handlers_;
    std::unordered_map<std::string, JSValue> command_handlers_;

    int error_count_ = 0;
    static constexpr int kMaxErrorsBeforeUnload = 50;
};

} // namespace grotto::plugin

namespace std {
template<>
struct is_error_code_enum<grotto::plugin::PluginError> : true_type {};
}
