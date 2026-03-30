#pragma once
#include <string>
#include <vector>

namespace grotto {

struct VoiceState {
    bool in_voice        = false;
    bool muted           = false;
    bool deafened        = false;
    std::string active_channel;              // channel or peer for 1:1 call
    std::vector<std::string> participants;   // user_ids in room
    std::vector<std::string> speaking_peers; // peers currently speaking (energy above threshold)
    std::string voice_mode = "ptt";          // "ptt" or "vox"
};

struct RuntimeVoiceIceConfig {
    std::vector<std::string> ice_servers;
    std::string turn_username;
    std::string turn_password;
    bool from_server = false;
};

} // namespace grotto
