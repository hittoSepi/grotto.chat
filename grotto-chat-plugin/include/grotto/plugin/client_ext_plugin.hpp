#pragma once

#include "grotto/plugin/plugin_instance.hpp"

namespace grotto::plugin {

class ClientExtPlugin : public PluginInstance {
public:
    ClientExtPlugin(PluginMeta meta, PluginContext& ctx);
    ~ClientExtPlugin() override;

    void shutdown() override;

protected:
    void setup_type_specific_api() override;
};

} // namespace grotto::plugin
