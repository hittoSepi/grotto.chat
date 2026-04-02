#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace grotto::ui {

struct StatusInfo {
    bool        connected      = false;
    bool        connecting     = false;
    std::string local_user_id;
    std::string active_channel;
    bool        in_voice       = false;
    bool        muted          = false;
    bool        deafened       = false;
    std::string voice_channel;
    std::string voice_mode     = "ptt";  // "ptt" or "vox"
    std::string ptt_key        = "F1";
    std::size_t voice_rtc_connected = 0;
    std::size_t voice_send_ready = 0;
    std::size_t voice_recv_ready = 0;
    std::vector<std::string> voice_participants;
    std::vector<std::string> speaking_peers;  // subset of participants currently speaking
    std::vector<std::string> online_users;
};

ftxui::Element render_status_bar(const StatusInfo& info);

} // namespace grotto::ui
