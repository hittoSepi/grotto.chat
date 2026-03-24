#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace grotto::plugin {

struct PluginEvent {
    enum class Type {
        // All plugin types
        UserJoined,
        UserLeft,
        PresenceChanged,
        ChannelCreated,
        ChannelDeleted,

        // Bot + client_extension only
        MessageReceived,

        // Server_extension only
        ConnectionEstablished,
        ConnectionDropped,
        VoiceRoomJoined,
        VoiceRoomLeft,
        AuthSuccess,
        AuthFailure,
    };

    Type type;
    std::string channel;
    std::string user_id;
    std::string text;
    int64_t timestamp_ms = 0;

    std::unordered_map<std::string, std::string> extra;
};

const char* event_type_to_string(PluginEvent::Type type);

} // namespace grotto::plugin
