#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace grotto::plugin {

struct PluginCommand {
    std::string name;       // "/roll"
    std::string channel;    // which channel the command was issued in
    std::string sender_id;  // who issued the command
    std::vector<std::string> args; // "2d6" -> ["2d6"]
    int64_t timestamp_ms = 0;
};

} // namespace grotto::plugin
