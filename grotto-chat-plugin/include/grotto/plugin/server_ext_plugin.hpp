#pragma once

#include "grotto/plugin/plugin_instance.hpp"

namespace grotto::plugin {

class ServerExtPlugin : public PluginInstance {
public:
    ServerExtPlugin(PluginMeta meta, PluginContext& ctx);
    ~ServerExtPlugin() override;

    void shutdown() override;
    void on_event(const PluginEvent& event) override;

protected:
    void setup_type_specific_api() override;
};

} // namespace grotto::plugin
