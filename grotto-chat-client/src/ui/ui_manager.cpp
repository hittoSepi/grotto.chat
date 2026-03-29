#include "ui/ui_manager.hpp"
#include "ui/color_scheme.hpp"
#include "ui/tab_bar.hpp"
#include "ui/status_bar.hpp"
#include "ui/message_view.hpp"
#include "ui/layout.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/flexbox_config.hpp>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <regex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace ftxui;

namespace grotto::ui {

UIManager::UIManager(AppState& state, ClientConfig& cfg)
    : state_(state), cfg_(cfg) {
    // Initialize user list panel config from persisted settings
    user_list_config_.width = cfg.ui.user_list_width;
    user_list_config_.collapsed = cfg.ui.user_list_collapsed;
}

void UIManager::toggle_user_list() {
    user_list_config_.collapsed = !user_list_config_.collapsed;
    // Persist the change
    cfg_.ui.user_list_collapsed = user_list_config_.collapsed;
}

// ============================================================================
// MOUSE HANDLING IMPLEMENTATION
// ============================================================================

bool UIManager::handle_mouse_event(const Event& event) {
    if (!event.is_mouse()) return false;
    
    // event.mouse() returns non-const reference, but event is const
    // We need to get mouse data differently
    // FTXUI's Event has mouse() method that returns reference to mouse data
    // Since event is const, we need to use the non-const version or make a copy
    // Actually, let's use the event's internal mouse data via a workaround
    // FTXUI Event stores mouse data that we can access
    
    // Get mouse data by accessing through a temporary non-const reference
    // This is safe because we're only reading
    Event& evt = const_cast<Event&>(event);
    auto& mouse = evt.mouse();
    mouse_tracker_.set_position(mouse.x, mouse.y);
    
    // Handle mouse moved
    if (mouse.motion == Mouse::Moved) {
        mouse_tracker_.set_hover(mouse.x, mouse.y);
        
        // Handle ongoing panel resize drag
        if (is_resizing_panel_) {
            update_panel_resize(mouse.x);
            return true;
        }
        
        // Handle ongoing text selection
        if (mouse_tracker_.is_selecting()) {
            mouse_tracker_.update_selection(mouse.x, mouse.y);
            return true;
        }
        
        return false;  // Let FTXUI handle hover visual updates
    }
    
    // Handle button press
    if (mouse.button == Mouse::Left && mouse.motion == Mouse::Pressed) {
        // Record click for double/triple click detection
        mouse_tracker_.record_click(mouse.x, mouse.y);
        
        // Check for panel resize drag start (on divider)
        if (mouse_tracker_.is_over_panel_divider() && !user_list_config_.collapsed) {
            start_panel_resize(mouse.x);
            return true;
        }
        
        // Handle multi-click
        if (mouse_tracker_.is_triple_click()) {
            handle_triple_click(mouse.x, mouse.y);
            mouse_tracker_.reset_click_count();
            return true;
        } else if (mouse_tracker_.is_double_click()) {
            handle_double_click(mouse.x, mouse.y);
            return true;
        }
        
        // Single click
        handle_click(mouse.x, mouse.y, false);
        return true;
    }
    
    // Handle right click
    if (mouse.button == Mouse::Right && mouse.motion == Mouse::Pressed) {
        handle_click(mouse.x, mouse.y, true);
        return true;
    }
    
    // Handle button release
    if (mouse.button == Mouse::Left && mouse.motion == Mouse::Released) {
        if (is_resizing_panel_) {
            end_panel_resize();
            return true;
        }
        if (mouse_tracker_.is_selecting()) {
            // Copy selected messages to clipboard on mouse release
            auto sel = mouse_tracker_.selection_region();
            if (sel.height > 0 && mouse_tracker_.is_over_message_area()) {
                auto ch = state_.active_channel();
                if (ch) {
                    auto ch_state = state_.channel_snapshot(*ch);
                    int visible_rows = mouse_tracker_.message_region().height;
                    int message_width = mouse_tracker_.message_region().width;
                    if (!user_list_config_.collapsed) {
                        int panel_width = get_panel_width(user_list_config_, screen_.dimx());
                        message_width = std::max(1, screen_.dimx() - panel_width - 1);
                    }
                    auto visible = collect_visible_message_lines(
                        ch_state, cfg_.ui.timestamp_format, visible_rows, message_width);
                    if (!visible.empty()) {
                        int start_line = std::max(0, sel.y - mouse_tracker_.message_region().y);
                        int end_line = std::min(static_cast<int>(visible.size()) - 1,
                                                start_line + sel.height - 1);
                        std::string selected;
                        for (int i = start_line; i <= end_line; ++i) {
                            if (!selected.empty()) selected += '\n';
                            selected += visible[i].plain_text;
                        }
                        if (!selected.empty()) {
                            copy_to_clipboard(selected);
                        }
                    }
                }
            }
            mouse_tracker_.end_selection();
            return true;
        }
    }
    
    // Handle mouse wheel
    if (mouse.button == Mouse::WheelUp || mouse.button == Mouse::WheelDown) {
        if (auto ch = state_.active_channel()) {
            if (mouse.button == Mouse::WheelUp) {
                state_.scroll_up(*ch, MouseConfig::kWheelScrollLines);
            } else {
                state_.scroll_down(*ch, MouseConfig::kWheelScrollLines);
            }
            return true;
        }
    }
    
    return false;
}



void UIManager::handle_click(int mouse_x, int mouse_y, bool is_right_click) {
    // Check tab bar clicks
    int tab_idx = get_tab_index_at_position(mouse_x, mouse_y);
    if (tab_idx >= 0) {
        // Channel switch via mouse
        auto channels = state_.channel_list();
        if (tab_idx < static_cast<int>(channels.size())) {
            state_.set_active_channel(channels[tab_idx]);
            state_.mark_read(channels[tab_idx]);
        }
        return;
    }
    
    // Check user list toggle button
    if (is_mouse_on_user_list_toggle(mouse_x, mouse_y)) {
        toggle_user_list();
        return;
    }
    
    // Check user list clicks
    if (!user_list_config_.collapsed && mouse_tracker_.is_over_user_list()) {
        auto user = get_user_at_position(mouse_x, mouse_y);
        if (user) {
            // Left or right click on user - mention them
            std::string mention = "@" + *user + " ";
            input_line_.insert_text(mention);
        }
        return;
    }
    
    // Check message area clicks (start text selection / open URL)
    if (mouse_tracker_.is_over_message_area()) {
        if (is_right_click) {
            // Right-click: open the most recently visible URL.
            // We can't accurately map click Y to message index when messages wrap,
            // so scan all visible messages from bottom up and open the first URL found.
            auto ch = state_.active_channel();
            if (ch) {
                auto ch_state = state_.channel_snapshot(*ch);
                int total = static_cast<int>(ch_state.messages.size());
                if (total > 0) {
                    int visible_rows = mouse_tracker_.message_region().height;
                    int bottom_idx = total - 1 - ch_state.scroll_offset;
                    bottom_idx = std::clamp(bottom_idx, 0, total - 1);
                    int top_idx = std::max(0, bottom_idx - visible_rows + 1);

                    // Estimate clicked message — use Y position as a hint
                    int line_y = mouse_y - mouse_tracker_.message_region().y;
                    int hint_idx = std::clamp(top_idx + line_y, top_idx, bottom_idx);

                    static const std::regex url_re(R"(https?://\S{3,})");
                    // Search from hint outward across all visible messages
                    for (int delta = 0; delta <= (bottom_idx - top_idx); ++delta) {
                        for (int d : {delta, -delta}) {
                            if (d != 0 && d == -delta) continue; // avoid duplicating delta=0
                            int idx = hint_idx + d;
                            if (idx < top_idx || idx > bottom_idx) continue;
                            std::smatch m;
                            if (std::regex_search(ch_state.messages[idx].content, m, url_re)) {
                                open_url(m[0].str());
                                return;
                            }
                        }
                    }
                }
            }
        } else {
            mouse_tracker_.start_selection(mouse_x, mouse_y);
        }
        return;
    }
}

void UIManager::handle_double_click(int mouse_x, int mouse_y) {
    (void)mouse_x;
    // Double-click to select word (entire message in this implementation)
    if (mouse_tracker_.is_over_message_area()) {
        int line_y = mouse_y - mouse_tracker_.message_region().y;
        auto ch = state_.active_channel();
        if (!ch) return;
        
        auto ch_state = state_.channel_snapshot(*ch);
        int visible_rows = mouse_tracker_.message_region().height;
        int message_width = mouse_tracker_.message_region().width;
        if (!user_list_config_.collapsed) {
            int panel_width = get_panel_width(user_list_config_, screen_.dimx());
            message_width = std::max(1, screen_.dimx() - panel_width - 1);
        }
        auto visible = collect_visible_message_lines(
            ch_state, cfg_.ui.timestamp_format, visible_rows, message_width);
        if (line_y >= 0 && line_y < static_cast<int>(visible.size())) {
            copy_to_clipboard(visible[line_y].plain_text);
        }
    }
}

void UIManager::handle_triple_click(int mouse_x, int mouse_y) {
    (void)mouse_x;
    // Triple-click to select entire line/message with sender
    if (mouse_tracker_.is_over_message_area()) {
        int line_y = mouse_y - mouse_tracker_.message_region().y;
        auto ch = state_.active_channel();
        if (!ch) return;
        
        auto ch_state = state_.channel_snapshot(*ch);
        int visible_rows = mouse_tracker_.message_region().height;
        int message_width = mouse_tracker_.message_region().width;
        if (!user_list_config_.collapsed) {
            int panel_width = get_panel_width(user_list_config_, screen_.dimx());
            message_width = std::max(1, screen_.dimx() - panel_width - 1);
        }
        auto visible = collect_visible_message_lines(
            ch_state, cfg_.ui.timestamp_format, visible_rows, message_width);
        if (line_y >= 0 && line_y < static_cast<int>(visible.size())) {
            copy_to_clipboard(visible[line_y].plain_text);
        }
    }
}

int UIManager::get_tab_index_at_position(int mouse_x, int mouse_y) const {
    if (!mouse_tracker_.tab_bar_region().contains(mouse_x, mouse_y)) {
        return -1;
    }
    
    // Find which tab was clicked based on stored positions
    int rel_x = mouse_x - mouse_tracker_.tab_bar_region().x;
    
    for (size_t i = 0; i < tab_positions_.size(); ++i) {
        // Estimate tab width based on channel name length + padding + unread badge
        auto channels = state_.channel_list();
        if (i >= channels.size()) continue;
        
        int unread = state_.unread_count(channels[i]);
        int tab_width = static_cast<int>(channels[i].length()) + 2;  // + padding
        if (unread > 0) {
            tab_width += 3 + static_cast<int>(std::to_string(unread).length());  // [n]
        }
        
        const auto& [channel, x_pos] = tab_positions_[i];
        if (rel_x >= x_pos && rel_x < x_pos + tab_width) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

std::optional<std::string> UIManager::get_user_at_position(int mouse_x, int mouse_y) const {
    (void)mouse_x;  // Not used currently, but may be needed for future hit testing
    
    // Check each user position
    for (const auto& [user_id, y_pos] : user_positions_) {
        if (mouse_y == y_pos) {
            return user_id;
        }
        // Allow some tolerance
        if (std::abs(mouse_y - y_pos) <= 1) {
            return user_id;
        }
    }
    return std::nullopt;
}

bool UIManager::is_mouse_on_panel_divider(int mouse_x, int mouse_y) const {
    (void)mouse_x;
    (void)mouse_y;
    return mouse_tracker_.is_over_panel_divider();
}

bool UIManager::is_mouse_on_user_list_toggle(int mouse_x, int mouse_y) const {
    // Toggle button is in the top-right corner of the tab bar
    // For simplicity, check if clicking near the right edge of the tab bar
    if (!mouse_tracker_.tab_bar_region().contains(mouse_x, mouse_y)) {
        return false;
    }
    // Check if clicking near the right edge
    int right_edge = mouse_tracker_.tab_bar_region().right();
    return mouse_x >= right_edge - 4;  // Last 4 characters
}

void UIManager::start_panel_resize(int mouse_x) {
    is_resizing_panel_ = true;
    resize_start_x_ = mouse_x;
    resize_start_width_ = user_list_config_.width;
    mouse_tracker_.start_drag(mouse_x, 0);
}

void UIManager::update_panel_resize(int mouse_x) {
    if (!is_resizing_panel_) return;
    
    int delta = resize_start_x_ - mouse_x;  // Invert: dragging left increases width
    int new_width = resize_start_width_ + delta;
    
    // Clamp to reasonable bounds
    new_width = std::clamp(new_width, MouseConfig::kMinPanelWidth, screen_.dimx() / 2);
    
    user_list_config_.width = new_width;
    cfg_.ui.user_list_width = new_width;
}

void UIManager::end_panel_resize() {
    is_resizing_panel_ = false;
    mouse_tracker_.end_drag();
}

void UIManager::start_text_selection(int mouse_x, int mouse_y) {
    mouse_tracker_.start_selection(mouse_x, mouse_y);
}

void UIManager::update_text_selection(int mouse_x, int mouse_y) {
    mouse_tracker_.update_selection(mouse_x, mouse_y);
}

void UIManager::end_text_selection() {
    mouse_tracker_.end_selection();
    
    // Extract selected text from messages
    auto region = mouse_tracker_.selection_region();
    if (region.height <= 0) {
        return;
    }
    auto ch = state_.active_channel();
    if (!ch) {
        return;
    }
    auto ch_state = state_.channel_snapshot(*ch);
    int visible_rows = mouse_tracker_.message_region().height;
    int message_width = mouse_tracker_.message_region().width;
    if (!user_list_config_.collapsed) {
        int panel_width = get_panel_width(user_list_config_, screen_.dimx());
        message_width = std::max(1, screen_.dimx() - panel_width - 1);
    }
    auto visible = collect_visible_message_lines(
        ch_state, cfg_.ui.timestamp_format, visible_rows, message_width);
    if (visible.empty()) {
        return;
    }
    int start_line = std::max(0, region.y - mouse_tracker_.message_region().y);
    int end_line = std::min(static_cast<int>(visible.size()) - 1,
                            start_line + region.height - 1);
    std::string selected;
    for (int i = start_line; i <= end_line; ++i) {
        if (!selected.empty()) {
            selected += '\n';
        }
        selected += visible[i].plain_text;
    }
    if (!selected.empty()) {
        copy_to_clipboard(selected);
    }
}

void UIManager::copy_selection_to_clipboard() {
    // If there is no active mouse selection, copy the input line contents
    std::string text = input_line_.text();
    if (!text.empty()) {
        copy_to_clipboard(text);
    }
}

void UIManager::notify() {
    screen_.PostEvent(Event::Custom);
}

void UIManager::request_exit() {
    screen_.ExitLoopClosure()();
}

void UIManager::push_system_msg(const std::string& txt) {
    auto ch = state_.active_channel().value_or("server");
    push_system_msg_to_channel(ch, txt, false);
}

void UIManager::push_system_msg_to_channel(const std::string& channel_id,
                                           const std::string& txt,
                                           bool activate_channel) {
    std::string ch = channel_id.empty() ? "server" : channel_id;
    Message msg;
    msg.type         = Message::Type::System;
    msg.content      = txt;
    msg.sender_id    = "system";
    msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    state_.post_ui([this, ch = std::move(ch), m = std::move(msg), activate_channel]() mutable {
        state_.ensure_channel(ch);
        if (activate_channel) {
            state_.set_active_channel(ch);
        }
        state_.push_message(ch, std::move(m));
    });
    notify();
}

Element UIManager::build_main_content(const std::string& active_ch, int msg_rows, int term_cols) {
    // Ensure channel users are populated (from online users if not already set)
    if (!active_ch.empty()) {
        state_.ensure_channel_users_from_online(active_ch);
    }

    int panel_width = 0;
    int msg_width = term_cols;
    if (!user_list_config_.collapsed) {
        panel_width = get_panel_width(user_list_config_, term_cols);
        msg_width = std::max(1, term_cols - panel_width - 1);  // -1 for separator
    }
    
    // Message area
    Element msg_el;
    if (!active_ch.empty()) {
        auto ch_state = state_.channel_snapshot(active_ch);
        msg_el = render_messages(ch_state, cfg_.ui.timestamp_format, msg_rows, msg_width);
    } else {
        msg_el = text("") | flex;
    }
    
    // If user list is collapsed, just return the message view
    if (user_list_config_.collapsed) {
        mouse_tracker_.set_user_list_region({0, 0, 0, 0});  // Clear user list region
        mouse_tracker_.set_panel_divider_region({0, 0, 0, 0});
        return msg_el | flex;
    }
    
    // Get user list for the active channel
    auto users = state_.channel_users(active_ch);
    
    // Build voice section from current voice state
    auto vs = state_.voice_snapshot();
    VoiceSection voice_sec;
    if (vs.in_voice && !vs.participants.empty()) {
        // Build voice section with current speaking/muted status
        std::vector<std::string> muted_users;
        if (vs.muted) {
            muted_users.push_back(state_.local_user_id());
        }
        voice_sec = build_voice_section(users, vs.participants, vs.speaking_peers, muted_users);
    }
    
    // Update user list region for mouse hit testing
    user_positions_.clear();
    int user_list_x = msg_width + 1;  // After message area and separator
    mouse_tracker_.set_user_list_region({mouse_tracker_.message_region().y, user_list_x, msg_rows, panel_width});
    mouse_tracker_.set_panel_divider_region({mouse_tracker_.message_region().y, user_list_x - 1, msg_rows, 1});
    panel_divider_x_ = user_list_x - 1;
    
    // Render user list panel with position tracking
    auto user_list_el = render_user_list_panel(users, voice_sec, user_list_config_, 
                                                  state_.local_user_id(), user_positions_, 
                                                  panel_divider_x_, mouse_tracker_.message_region().y);
    
    // Combine message view and user list panel horizontally.
    // HEIGHT LESS_THAN keeps a long user list from overflowing the content area.
    return hbox({
        msg_el | flex,
        separator(),
        user_list_el | size(WIDTH, EQUAL, panel_width)
                     | size(HEIGHT, LESS_THAN, msg_rows + 1),
    });
}

Element UIManager::build_document(int term_rows) {
    int term_cols = screen_.dimx() > 0 ? screen_.dimx() : 80;
    
    // Update region tracking for mouse hit testing
    mouse_tracker_.set_tab_bar_region({0, 0, kTabBarHeight, term_cols});
    
    // Compute visual input line count (handles wrapping + explicit newlines)
    auto count_visual_lines = [](const std::string& txt, int cols) -> int {
        if (cols <= 0) cols = 80;
        int lines = 1, col = 2; // 2 for "> " prefix
        for (size_t i = 0; i < txt.size(); ) {
            unsigned char c = static_cast<unsigned char>(txt[i]);
            if (c == '\n') { lines++; col = 0; i++; continue; }
            int cp_len = 1;
            if ((c & 0xE0) == 0xC0) cp_len = 2;
            else if ((c & 0xF0) == 0xE0) cp_len = 3;
            else if ((c & 0xF8) == 0xF0) cp_len = 4;
            col++;
            if (col > cols) { lines++; col = 1; }
            i += cp_len;
        }
        return lines;
    };
    int input_lines = std::max(1, std::min(5, count_visual_lines(input_line_.text(), term_cols)));
    int msg_rows = std::max(1, term_rows - kTabBarHeight - kStatusBarHeight - input_lines - 2);  // -2 for separators
    int msg_y = kTabBarHeight + 1;  // After tab bar and separator
    
    int status_y = msg_y + msg_rows + 1;
    int input_y = status_y + kStatusBarHeight + 1;
    
    mouse_tracker_.set_message_region({msg_y, 0, msg_rows, term_cols});
    mouse_tracker_.set_status_bar_region({status_y, 0, kStatusBarHeight, term_cols});
    mouse_tracker_.set_input_region({input_y, 0, input_lines, term_cols});
    
    // Panels
    auto channels  = state_.channel_list();
    auto active_ch = state_.active_channel().value_or("");

    // Tab bar with position tracking for mouse hit testing
    std::vector<int> unread;
    for (auto& c : channels) unread.push_back(state_.unread_count(c));
    tab_positions_.clear();
    auto tab_inner = render_tab_bar(channels, active_ch, unread, tab_positions_);
    // Append the user-list toggle button to the right of the tab bar
    auto tab_el = hbox({
        tab_inner | flex,
        render_toggle_button(user_list_config_.collapsed),
    }) | bgcolor(palette::bg_dark());

    // Build main content (messages + user list panel)
    auto main_content = build_main_content(active_ch, msg_rows, term_cols);

    // Status bar
    StatusInfo si;
    si.connected      = state_.connected();
    si.local_user_id  = state_.local_user_id();
    si.active_channel = active_ch;
    auto vs           = state_.voice_snapshot();
    si.in_voice       = vs.in_voice;
    si.muted          = vs.muted;
    si.deafened       = vs.deafened;
    si.voice_channel  = vs.active_channel;
    si.voice_mode     = vs.voice_mode;
    si.voice_participants = vs.participants;
    si.speaking_peers = vs.speaking_peers;
    si.online_users   = state_.online_users();
    auto status_el = render_status_bar(si);

    // Input line — render with proper wrapping and multiline support
    std::string input_text = input_line_.text();
    int byte_off = input_line_.cursor_byte_offset();

    // Build full display string: "> " + input text
    std::string full = "> " + input_text;
    int cursor_in_full = 2 + byte_off; // byte offset of cursor in 'full'

    // Split into visual lines (at explicit \n and wrapping at term_cols)
    struct VLine { int start; int end; }; // byte offsets in 'full'
    std::vector<VLine> vlines;
    int col = 0, line_start = 0;
    for (int i = 0; i < static_cast<int>(full.size()); ) {
        unsigned char c = static_cast<unsigned char>(full[i]);
        if (c == '\n') {
            vlines.push_back({line_start, i});
            i++;
            line_start = i;
            col = 0;
            continue;
        }
        int cp_len = 1;
        if ((c & 0xE0) == 0xC0) cp_len = 2;
        else if ((c & 0xF0) == 0xE0) cp_len = 3;
        else if ((c & 0xF8) == 0xF0) cp_len = 4;
        col++;
        if (col > term_cols) {
            vlines.push_back({line_start, i});
            line_start = i;
            col = 1;
        }
        i += cp_len;
    }
    vlines.push_back({line_start, static_cast<int>(full.size())});

    // Render each visual line, placing cursor on the correct one
    Elements line_els;
    bool cursor_placed = false;
    for (auto& vl : vlines) {
        std::string line_text = full.substr(vl.start, vl.end - vl.start);

        if (!cursor_placed && cursor_in_full >= vl.start && cursor_in_full <= vl.end) {
            cursor_placed = true;
            int rel = cursor_in_full - vl.start;

            if (rel < static_cast<int>(line_text.size())) {
                // Cursor is within this line
                std::string b = line_text.substr(0, rel);
                unsigned char lead = static_cast<unsigned char>(line_text[rel]);
                int cp_len = 1;
                if ((lead & 0xE0) == 0xC0) cp_len = 2;
                else if ((lead & 0xF0) == 0xE0) cp_len = 3;
                else if ((lead & 0xF8) == 0xF0) cp_len = 4;
                std::string a = line_text.substr(rel, cp_len);
                std::string r = line_text.substr(rel + cp_len);
                line_els.push_back(hbox({
                    text(b) | color(palette::fg()),
                    text(a) | color(palette::bg()) | bgcolor(palette::fg()),
                    text(r) | color(palette::fg()),
                }));
            } else {
                // Cursor at end of this line
                line_els.push_back(hbox({
                    text(line_text) | color(palette::fg()),
                    text(" ") | color(palette::bg()) | bgcolor(palette::fg()),
                }));
            }
        } else {
            line_els.push_back(text(line_text) | color(palette::fg()));
        }
    }

    if (line_els.empty()) {
        line_els.push_back(hbox({
            text("> ") | color(palette::fg()),
            text(" ") | color(palette::bg()) | bgcolor(palette::fg()),
        }));
    }

    auto input_el = vbox(std::move(line_els))
                    | bgcolor(palette::bg()) | size(HEIGHT, LESS_THAN, input_lines + 1);

    return vbox({
        tab_el,
        separator(),
        main_content | flex,
        separator(),
        status_el,
        separator(),
        input_el,
    }) | bgcolor(palette::bg());
}

void UIManager::run(SubmitFn on_submit,
                    std::function<void()> on_quit,
                    std::function<void(int)> on_channel_switch,
                    ChannelCycleFn on_channel_cycle,
                    PttToggleFn on_ptt_toggle,
                    OpenSettingsFn on_open_settings) {
    screen_.ForceHandleCtrlC(true);  // Let app handle Ctrl+C via CatchEvent (\x03)

#ifdef _WIN32
    // Disable QuickEdit mode so right-click doesn't open the system context menu.
    // This allows FTXUI to receive right-click mouse events properly.
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hin, &mode)) {
        SetConsoleMode(hin, mode & ~ENABLE_QUICK_EDIT_MODE);
    }
#endif

    auto renderer = Renderer([&] {
        // Drain cross-thread UI updates before rendering
        state_.drain_ui_queue();

        int rows = screen_.dimx() > 0 ? screen_.dimy() : 24;
        return build_document(rows);
    });

    auto event_handler = CatchEvent(renderer, [&](Event event) -> bool {
        // Handle mouse events first
        if (event.is_mouse()) {
            return handle_mouse_event(event);
        }
        
        if (event == Event::Custom) {
            // Posted by notify() — just triggers a redraw
            return true;
        }
        if (event.input() == "\x16") {
            if (auto clipboard = read_from_clipboard()) {
                tab_completer_.reset();
                input_line_.insert_text(*clipboard);
            }
            return true;
        }
        if (event.input() == "\x03") {
            copy_selection_to_clipboard();
            return true;
        }
        if (event.input() == "\x04") {
            return true;
        }
        // F1 — PTT toggle (since FTXUI doesn't support key-release, use toggle)
        if (event == Event::F1) {
            ptt_toggled_ = !ptt_toggled_;
            if (on_ptt_toggle) on_ptt_toggle();
            return true;
        }
        // F12 — Open settings
        if (event == Event::F12) {
            if (on_open_settings) on_open_settings();
            return true;
        }
        // F2 — Toggle user list panel
        if (event == Event::F2) {
            toggle_user_list();
            return true;
        }
        // Insert newline: Alt+N (works on Windows Terminal),
        // Alt+Enter (works on terminals that don't capture it),
        // Shift+Enter / Ctrl+Enter (CSI-u / kitty protocol)
        if (event.input() == "\x1bn" || event.input() == "\x1bN" ||
            event.input() == "\x1b\r" || event.input() == "\x1b\n" ||
            event.input() == "\x1b[13;2u" || event.input() == "\x1b[13;5u") {
            tab_completer_.reset();
            input_line_.insert(U'\n');
            return true;
        }
        if (event == Event::Return) {
            tab_completer_.reset();
            std::string line = input_line_.commit();
            if (!line.empty() && on_submit) on_submit(line);
            return true;
        }
        if (event == Event::Backspace) {
            tab_completer_.reset();
            input_line_.backspace();
            return true;
        }
        if (event == Event::Delete) {
            tab_completer_.reset();
            input_line_.del_forward();
            return true;
        }
        if (event == Event::ArrowLeft) {
            input_line_.move_left();
            return true;
        }
        if (event == Event::ArrowRight) {
            input_line_.move_right();
            return true;
        }
        if (event == Event::ArrowUp) {
            if (!input_line_.move_up()) {
                input_line_.history_prev();
            }
            return true;
        }
        if (event == Event::ArrowDown) {
            if (!input_line_.move_down()) {
                input_line_.history_next();
            }
            return true;
        }
        if (event == Event::Home) {
            input_line_.move_home();
            return true;
        }
        // Ctrl+A — move to beginning of line (readline: move-beginning-of-line)
        if (event.input() == "\x01") {
            input_line_.move_home();
            return true;
        }
        // Ctrl+E — move to end of line (readline: move-end-of-line)
        if (event.input() == "\x05") {
            input_line_.move_end();
            return true;
        }
        // Ctrl+K — kill from cursor to end of line (readline: kill-line)
        if (event.input() == "\x0b") {
            tab_completer_.reset();
            input_line_.kill_to_end();
            return true;
        }
        // Ctrl+U — kill whole input line (readline: unix-line-discard)
        if (event.input() == "\x15") {
            tab_completer_.reset();
            input_line_.clear();
            return true;
        }
        // Ctrl+W — delete word backward (readline: unix-word-rubout)
        if (event.input() == "\x17") {
            tab_completer_.reset();
            input_line_.delete_word_backward();
            return true;
        }
        if (event == Event::End) {
            input_line_.move_end();
            return true;
        }
        if (event == Event::PageUp) {
            if (auto ch = state_.active_channel()) state_.scroll_up(*ch, 10);
            return true;
        }
        if (event == Event::PageDown) {
            if (auto ch = state_.active_channel()) state_.scroll_down(*ch, 10);
            return true;
        }
        if (event == Event::Escape) {
            if (on_quit) on_quit();
            screen_.ExitLoopClosure()();
            return true;
        }
        if (event.is_character()) {
            std::string text = event.character();
            if (!text.empty()) {
                tab_completer_.reset();
                input_line_.insert_text(text);
                return true;
            }
        }
        // Tab completion
        if (event.input() == "\t") {
            auto completed = tab_completer_.complete(
                input_line_.text(),
                state_.online_users(),
                state_.channel_list(),
                known_commands());
            input_line_.set_text(completed);
            return true;
        }
        // Alt+1..9 — switch to channel by 0-based index
        if (event.input().size() == 2 &&
            event.input()[0] == '\x1b' &&
            event.input()[1] >= '1' && event.input()[1] <= '9') {
            if (on_channel_switch)
                on_channel_switch(event.input()[1] - '1');
            return true;
        }
        if (event.input() == "\x1b[1;3D" || event.input() == "\x1b\x1b[D") {
            if (on_channel_cycle) on_channel_cycle(-1);
            return true;
        }
        if (event.input() == "\x1b[1;3C" || event.input() == "\x1b\x1b[C") {
            if (on_channel_cycle) on_channel_cycle(1);
            return true;
        }
        return false;
    });

    screen_.Loop(event_handler);
}

} // namespace grotto::ui
