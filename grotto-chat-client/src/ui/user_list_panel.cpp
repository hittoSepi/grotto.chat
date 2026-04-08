#include "ui/user_list_panel.hpp"
#include "ui/color_scheme.hpp"
#include "i18n/strings.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>

using namespace ftxui;

namespace grotto::ui {

namespace {

// Voice status indicators
const char* kIndicatorTalking = "\U0001F7E2";  // 🟢 Green circle
const char* kIndicatorMuted   = "\U0001F7E1";  // 🟡 Yellow circle  
const char* kIndicatorOff     = "\u26AA";       // ⚪ White circle
const char* kPresenceDot      = "\u25CF";

Color presence_dot_color(PresenceStatus presence) {
    switch (presence) {
        case PresenceStatus::Online: return palette::online();
        case PresenceStatus::Away:   return palette::away_c();
        case PresenceStatus::Dnd:    return palette::dnd_c();
        case PresenceStatus::Offline:
        default:                     return palette::offline_c();
    }
}

Element render_user_entry(const ChannelUserInfo& user, const std::string& local_user_id) {
    std::string prefix = user.prefix();
    std::string display = prefix + user.user_id;
    
    // Color based on role
    Color name_color;
    if (user.role == UserRole::Admin) {
        name_color = palette::red();
    } else if (user.role == UserRole::Voice) {
        name_color = palette::green();
    } else {
        name_color = palette::fg();
    }

    if (user.presence == PresenceStatus::Away) {
        name_color = palette::yellow();
    } else if (user.presence == PresenceStatus::Dnd) {
        name_color = palette::red();
    }
    
    // Highlight local user
    if (user.user_id == local_user_id) {
        name_color = palette::cyan();
    }
    
    return hbox({
        text(" "),
        text(kPresenceDot) | color(presence_dot_color(user.presence)),
        text(" "),
        text(display) | color(name_color),
    });
}

Element render_voice_user(const std::string& user_id, ChannelUserInfo::VoiceStatus status,
                          const std::string& local_user_id) {
    const char* indicator = kIndicatorOff;
    Color indicator_color = palette::comment();
    
    switch (status) {
        case ChannelUserInfo::VoiceStatus::Talking:
            indicator = kIndicatorTalking;
            indicator_color = palette::online();  // Green
            break;
        case ChannelUserInfo::VoiceStatus::Muted:
            indicator = kIndicatorMuted;
            indicator_color = palette::yellow();
            break;
        case ChannelUserInfo::VoiceStatus::Off:
        default:
            indicator = kIndicatorOff;
            indicator_color = palette::comment();
            break;
    }
    
    Color name_color = (user_id == local_user_id) ? palette::cyan() : palette::fg();
    
    return hbox({
        text(" "),                             // 1-space left pad
        text(indicator) | color(indicator_color),
        text(" ") | color(palette::fg()),
        text(user_id) | color(name_color),
    });
}

} // anonymous namespace

Element render_toggle_button(bool collapsed) {
    // "<<" when panel is visible (click to collapse), ">>" when hidden (click to expand)
    // Padded so it reads as a button; F2 also toggles
    std::string label = collapsed ? " >> " : " << ";
    return text(label) | color(palette::blue()) | bold;
}

int get_panel_width(const UserListConfig& config, int term_cols) {
    if (config.collapsed) {
        return 0;  // Collapsed - no panel
    }
    // Clamp width to reasonable bounds
    int min_width = 15;
    int max_width = term_cols / 2;  // Max half the terminal
    return std::clamp(config.width, min_width, max_width);
}

std::vector<ChannelUserInfo> sort_users_by_role(std::vector<ChannelUserInfo> users) {
    std::sort(users.begin(), users.end(), [](const ChannelUserInfo& a, const ChannelUserInfo& b) {
        if (a.role != b.role) return static_cast<int>(a.role) > static_cast<int>(b.role);
        return a.user_id < b.user_id;
    });
    return users;
}

VoiceSection build_voice_section(
    const std::vector<ChannelUserInfo>& users,
    const std::vector<std::string>& voice_participants,
    const std::vector<std::string>& speaking_peers,
    const std::vector<std::string>& muted_users) {
    
    VoiceSection section;
    
    for (const auto& user : users) {
        // Check if user is in voice
        bool in_voice = std::find(voice_participants.begin(), voice_participants.end(), 
                                   user.user_id) != voice_participants.end();
        if (!in_voice) continue;
        
        bool is_speaking = std::find(speaking_peers.begin(), speaking_peers.end(),
                                      user.user_id) != speaking_peers.end();
        bool is_muted = std::find(muted_users.begin(), muted_users.end(),
                                   user.user_id) != muted_users.end();
        
        if (is_speaking) {
            section.talking_users.push_back(user.user_id);
        } else if (is_muted) {
            section.muted_users.push_back(user.user_id);
        } else {
            section.connected_users.push_back(user.user_id);
        }
    }
    
    return section;
}

Element render_user_list_panel(
    const std::vector<ChannelUserInfo>& users,
    const VoiceSection& voice_section,
    const UserListConfig& config,
    const std::string& local_user_id,
    std::vector<UserHitRegion>& out_user_positions,
    int& out_panel_divider_x,
    int base_x,
    int base_y) {
    
    out_user_positions.clear();
    int current_y = base_y;
    
    if (config.collapsed) {
        // When collapsed, we still need to track the divider position
        out_panel_divider_x = -1;  // No divider when collapsed
        return text("");  // Empty when collapsed
    }
    
    Elements panel_content;

    std::vector<ChannelUserInfo> online_users;
    std::vector<ChannelUserInfo> offline_users;
    online_users.reserve(users.size());
    offline_users.reserve(users.size());
    for (const auto& user : users) {
        if (user.presence == PresenceStatus::Offline) {
            offline_users.push_back(user);
        } else {
            online_users.push_back(user);
        }
    }
    
    // ── USERS: header ──────────────────────────────────────────────────────
    std::string users_header = i18n::tr(i18n::I18nKey::USERS_HEADER) +
                               std::to_string(online_users.size());
    panel_content.push_back(text(users_header) | bold | color(palette::fg_dark()));
    panel_content.push_back(separator() | color(palette::bg_highlight()));
    current_y += 2;  // Header + separator
    
    // ── Online users ───────────────────────────────────────────────────────
    for (const auto& user : online_users) {
        out_user_positions.push_back({user.user_id, base_x, current_y, config.width, 1});
        panel_content.push_back(render_user_entry(user, local_user_id));
        current_y++;
    }

    // ── Offline users section ──────────────────────────────────────────────
    if (config.show_offline && !offline_users.empty()) {
        panel_content.push_back(text(""));  // Spacer
        current_y++;
        std::string offline_header = i18n::tr(i18n::I18nKey::OFFLINE_HEADER) +
                                     std::to_string(offline_users.size());
        panel_content.push_back(text(offline_header) | bold | color(palette::comment()));
        panel_content.push_back(separator() | color(palette::bg_highlight()));
        current_y += 2;

        for (const auto& user : offline_users) {
            out_user_positions.push_back({user.user_id, base_x, current_y, config.width, 1});
            panel_content.push_back(render_user_entry(user, local_user_id) | color(palette::comment()));
            current_y++;
        }
    }
    
    // ── VOICE: section (if anyone is in voice) ─────────────────────────────
    int voice_count = static_cast<int>(voice_section.talking_users.size() +
                                        voice_section.muted_users.size() +
                                        voice_section.connected_users.size());
    if (voice_count > 0) {
        panel_content.push_back(text(""));  // Spacer
        current_y++;
        std::string voice_header = i18n::tr(i18n::I18nKey::VOICE_HEADER) + std::to_string(voice_count);
        panel_content.push_back(text(voice_header) | bold | color(palette::fg_dark()));
        panel_content.push_back(separator() | color(palette::bg_highlight()));
        current_y += 2;
        
        // Speaking users first (green)
        for (const auto& user_id : voice_section.talking_users) {
            out_user_positions.push_back({user_id, base_x, current_y, config.width, 1});
            panel_content.push_back(render_voice_user(user_id, 
                ChannelUserInfo::VoiceStatus::Talking, local_user_id));
            current_y++;
        }
        
        // Connected but not speaking (white circle)
        for (const auto& user_id : voice_section.connected_users) {
            out_user_positions.push_back({user_id, base_x, current_y, config.width, 1});
            panel_content.push_back(render_voice_user(user_id,
                ChannelUserInfo::VoiceStatus::Off, local_user_id));
            current_y++;
        }
        
        // Muted users (yellow)
        for (const auto& user_id : voice_section.muted_users) {
            out_user_positions.push_back({user_id, base_x, current_y, config.width, 1});
            panel_content.push_back(render_voice_user(user_id,
                ChannelUserInfo::VoiceStatus::Muted, local_user_id));
            current_y++;
        }
    }
    
    // Calculate panel divider position (left edge of the panel)
    // This is set by the caller based on layout
    out_panel_divider_x = config.width;
    
    // Build the panel — no box border (separator() in the parent hbox already
    // draws the left edge); use a subtle background tint to distinguish the
    // user-list area from the message view.
    auto panel = vbox(std::move(panel_content));

    return panel | bgcolor(palette::bg_dark());
}

} // namespace grotto::ui
