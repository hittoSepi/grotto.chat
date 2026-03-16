#pragma once

#include "grotto/plugin/plugin_instance.hpp"

namespace grotto::plugin {

class BotPluginInstance : public PluginInstance {
public:
    BotPluginInstance(PluginMeta meta, PluginContext& ctx);
    ~BotPluginInstance() override;

    void shutdown() override;

protected:
    void setup_type_specific_api() override;
};

} // namespace grotto::plugin
