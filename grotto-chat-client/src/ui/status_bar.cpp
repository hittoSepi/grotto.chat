#include "ui/status_bar.hpp"
#include "ui/color_scheme.hpp"
#include "i18n/strings.hpp"
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace grotto::ui {

Element render_status_bar(const StatusInfo& info) {
    // Left side
    Elements left;
    if (info.connected) {
        auto status_color = palette::online();
        if (info.local_presence == PresenceStatus::Away) {
            status_color = palette::away_c();
        } else if (info.local_presence == PresenceStatus::Dnd) {
            status_color = palette::dnd_c();
        }
        left.push_back(text("\u25CF ") | color(status_color));
    } else if (info.connecting) {
        left.push_back(text("\u25D4 ") | color(palette::yellow()));
    } else {
        left.push_back(text("\u25CB ") | color(palette::error_c()));
    }

    if (!info.local_user_id.empty()) {
        left.push_back(text(info.local_user_id) | color(palette::fg_dark()));
    }
    if (!info.connected && info.connecting) {
        left.push_back(text(" | " + i18n::tr(i18n::I18nKey::CONNECTING)) | color(palette::yellow()));
    }
    if (!info.active_channel.empty()) {
        left.push_back(text(" | " + info.active_channel) | color(palette::fg_dark()));
    }
    if (info.in_voice) {
        const std::string ptt_text = "PTT: " + (info.ptt_key.empty() ? std::string("F1") : info.ptt_key);
        std::string voice_text = "\U0001F3A4 " + info.voice_channel +
            " " + i18n::tr(i18n::I18nKey::USERS_COUNT, std::to_string(info.voice_participants.size()));
        if (info.muted) voice_text += " " + i18n::tr(i18n::I18nKey::MUTED_INDICATOR);
        if (info.deafened) voice_text += " " + i18n::tr(i18n::I18nKey::DEAFENED_INDICATOR);
        voice_text += " [" + std::string(info.voice_mode == "ptt" ? ptt_text : i18n::tr(i18n::I18nKey::VOX)) + "]";
        voice_text += " RTC " + std::to_string(info.voice_rtc_connected) + "/" + std::to_string(info.voice_participants.size());
        voice_text += " TX " + std::to_string(info.voice_send_ready) + "/" + std::to_string(info.voice_participants.size());
        voice_text += " RX " + std::to_string(info.voice_recv_ready) + "/" + std::to_string(info.voice_participants.size());

        left.push_back(text(" | ") | color(palette::fg_dark()));
        left.push_back(text(voice_text) | color(Color::Green));

        // Show speaking peers
        if (!info.speaking_peers.empty()) {
            std::string speaking_text = "\U0001F50A ";  // speaker icon
            for (size_t i = 0; i < info.speaking_peers.size(); ++i) {
                if (i > 0) speaking_text += ", ";
                speaking_text += info.speaking_peers[i];
            }
            left.push_back(text(" ") | color(palette::fg_dark()));
            left.push_back(text(speaking_text) | color(palette::cyan()));
        }
    }

    if (!info.transfer_summary.empty()) {
        left.push_back(text(" | ") | color(palette::fg_dark()));
        left.push_back(text(info.transfer_summary) | color(palette::cyan()));
    }

    Elements right;
    if (!info.typing_summary.empty()) {
        right.push_back(text(info.typing_summary) | color(palette::comment()));
    }

    return hbox({
        hbox(std::move(left)),
        filler(),
        hbox(std::move(right)),
    }) | bgcolor(palette::bg_dark());
}

} // namespace grotto::ui
