#pragma once
#include <ftxui/dom/elements.hpp>
#include "state/app_state.hpp"
#include <string>
#include <vector>

namespace grotto::ui {

struct StatusInfo {
    bool        connected      = false;
    bool        connecting     = false;
    grotto::PresenceStatus local_presence = grotto::PresenceStatus::Offline;
    std::string local_user_id;
    std::string active_channel;
    std::string connection_summary;
    bool        in_voice       = false;
    bool        voice_local_test = false;
    bool        muted          = false;
    bool        deafened       = false;
    bool        local_capture_active = false;
    std::string voice_channel;
    std::string voice_mode     = "toggle";  // "toggle", "hold", or "vox"
    std::string ptt_key        = "§";
    std::size_t voice_rtc_connected = 0;
    std::size_t voice_send_ready = 0;
    std::size_t voice_recv_ready = 0;
    std::vector<std::string> voice_participants;
    std::vector<std::string> speaking_peers;  // subset of participants currently speaking
    std::vector<std::string> online_users;
    std::string transfer_summary;
    std::string typing_summary;
};

ftxui::Element render_status_bar(const StatusInfo& info);

} // namespace grotto::ui
