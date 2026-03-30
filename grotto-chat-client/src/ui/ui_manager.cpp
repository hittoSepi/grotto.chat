#include "ui/ui_manager.hpp"
#include "ui/color_scheme.hpp"
#include "ui/tab_bar.hpp"
#include "ui/status_bar.hpp"
#include "ui/message_view.hpp"
#include "ui/terminal_image.hpp"
#include "ui/layout.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/flexbox_config.hpp>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <vector>

#include <stb_image.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef RGB
#undef RGB
#endif
#endif

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

using namespace ftxui;

namespace grotto::ui {

namespace {

#ifndef _WIN32
void swallow_interrupt_signal(int) {
}

void install_interrupt_handlers() {
    struct sigaction sa {};
    sa.sa_handler = swallow_interrupt_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGQUIT, &sa, nullptr);
}

class ScopedDisableTtyInterruptSignals {
public:
    ScopedDisableTtyInterruptSignals() {
        if (!isatty(STDIN_FILENO)) {
            return;
        }
        if (tcgetattr(STDIN_FILENO, &old_) != 0) {
            return;
        }
        termios raw = old_;
        raw.c_lflag &= ~ISIG;
#ifdef VINTR
        raw.c_cc[VINTR] = _POSIX_VDISABLE;
#endif
#ifdef VQUIT
        raw.c_cc[VQUIT] = _POSIX_VDISABLE;
#endif
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
            active_ = true;
        }
    }

    ~ScopedDisableTtyInterruptSignals() {
        if (active_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_);
        }
    }

private:
    bool active_ = false;
    termios old_{};
};
#endif

std::optional<std::string> extract_bracketed_paste(std::string_view input) {
    constexpr std::string_view kStart = "\x1b[200~";
    constexpr std::string_view kEnd = "\x1b[201~";
    if (!input.starts_with(kStart)) {
        return std::nullopt;
    }
    auto end_pos = input.rfind(kEnd);
    if (end_pos == std::string_view::npos || end_pos < kStart.size()) {
        return std::nullopt;
    }
    return std::string(input.substr(kStart.size(), end_pos - kStart.size()));
}

bool is_shift_insert_paste(std::string_view input) {
    return input == "\x1b[2;2~" || input == "\x1b[2~";
}

bool is_server_channel(std::string_view channel_id) {
    return channel_id == "server";
}

int effective_message_width(const MouseTracker& mouse_tracker,
                            const UserListConfig& user_list_config,
                            int term_cols) {
    if (mouse_tracker.message_region().width > 0) {
        return mouse_tracker.message_region().width;
    }

    if (user_list_config.collapsed) {
        return term_cols;
    }

    int panel_width = get_panel_width(user_list_config, term_cols);
    return std::max(1, term_cols - panel_width - 1);
}

std::vector<VisibleLayoutHit> current_visible_hits(AppState& state,
                                                   const MouseTracker& mouse_tracker,
                                                   const UserListConfig& user_list_config,
                                                   const ClientConfig& cfg,
                                                   int term_cols) {
    auto ch = state.active_channel();
    if (!ch) {
        return {};
    }

    auto ch_state = state.channel_snapshot(*ch);
    int visible_rows = mouse_tracker.message_region().height;
    int message_width = effective_message_width(mouse_tracker, user_list_config, term_cols);
    return collect_visible_layout_hits(
        ch_state, cfg.ui.timestamp_format, visible_rows, message_width);
}

std::optional<std::string> find_url_in_text(const std::string& text) {
    static const std::regex url_re(R"(((?:https?://|www\.)\S+))");
    std::smatch match;
    if (!std::regex_search(text, match, url_re)) {
        return std::nullopt;
    }

    std::string url = match[1].str();
    if (url.starts_with("www.")) {
        url = "https://" + url;
    }
    while (!url.empty() && (url.back() == ')' || url.back() == ']' || url.back() == '.' ||
                            url.back() == ',' || url.back() == ';')) {
        url.pop_back();
    }
    return url.empty() ? std::nullopt : std::optional<std::string>(url);
}

int visible_width(std::string_view text) {
    int width = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0x80) == 0) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        ++width;
    }
    return width;
}

size_t byte_offset_for_display_column(std::string_view text, int display_column) {
    if (display_column <= 0) {
        return 0;
    }

    int col = 0;
    size_t i = 0;
    while (i < text.size() && col < display_column) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t cp_len = 1;
        if ((c & 0x80) == 0) cp_len = 1;
        else if ((c & 0xE0) == 0xC0) cp_len = 2;
        else if ((c & 0xF0) == 0xE0) cp_len = 3;
        else if ((c & 0xF8) == 0xF0) cp_len = 4;
        i += cp_len;
        ++col;
    }
    return i;
}

std::string substring_by_display_columns(const std::string& text, int start_col, int end_col) {
    if (end_col <= start_col) {
        return {};
    }

    const int text_cols = visible_width(text);
    const int clamped_start = std::clamp(start_col, 0, text_cols);
    const int clamped_end = std::clamp(end_col, clamped_start, text_cols);
    if (clamped_end <= clamped_start) {
        return {};
    }

    const size_t start_byte = byte_offset_for_display_column(text, clamped_start);
    const size_t end_byte = byte_offset_for_display_column(text, clamped_end);
    if (end_byte <= start_byte || start_byte >= text.size()) {
        return {};
    }
    return text.substr(start_byte, end_byte - start_byte);
}

std::optional<std::filesystem::path> find_daemon_avatar_resource() {
    std::vector<std::filesystem::path> candidates{
        std::filesystem::path("resources") / "grotto-daemon-avatar.png",
        std::filesystem::path("grotto-chat-client") / "resources" / "grotto-daemon-avatar.png",
    };

    std::error_code cwd_ec;
    const auto cwd = std::filesystem::current_path(cwd_ec);
    if (!cwd_ec) {
        candidates.push_back(cwd / "resources" / "grotto-daemon-avatar.png");
        candidates.push_back(cwd / "grotto-chat-client" / "resources" / "grotto-daemon-avatar.png");
    }

    for (const auto& path : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && !ec) {
            return path;
        }
    }
    return std::nullopt;
}

std::optional<InlineImageThumbnail> load_thumbnail_from_png_file(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* rgba = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!rgba || width <= 0 || height <= 0) {
        if (rgba) {
            stbi_image_free(rgba);
        }
        return std::nullopt;
    }

    InlineImageThumbnail thumbnail;
    thumbnail.width = width;
    thumbnail.height = height;
    const size_t bytes = static_cast<size_t>(width * height * 4);
    thumbnail.rgba.assign(rgba, rgba + bytes);
    stbi_image_free(rgba);
    return thumbnail;
}

const std::shared_ptr<InlineImageThumbnail>& server_daemon_avatar_thumbnail() {
    static const std::shared_ptr<InlineImageThumbnail> cached = []() -> std::shared_ptr<InlineImageThumbnail> {
        auto path = find_daemon_avatar_resource();
        if (!path) {
            return {};
        }
        auto thumbnail = load_thumbnail_from_png_file(*path);
        if (!thumbnail) {
            return {};
        }
        return std::make_shared<InlineImageThumbnail>(std::move(*thumbnail));
    }();
    return cached;
}

std::optional<GraphicsDrawCommand> make_server_background_command(int viewport_x,
                                                                  int viewport_y,
                                                                  int viewport_width,
                                                                  int viewport_height) {
    if (viewport_width < 20 || viewport_height < 8) {
        return std::nullopt;
    }

    const auto& image = server_daemon_avatar_thumbnail();
    if (!image || image->width <= 0 || image->height <= 0 || image->rgba.empty()) {
        return std::nullopt;
    }

    const auto backend = active_graphics_backend_kind();
    if (backend == GraphicsBackendKind::ColorBlocks) {
        return std::nullopt;
    }

    // Watermark-style placement: intentionally small and tucked in top-right.
    const int max_columns_fit = std::max(8, viewport_width - 6);
    int columns = std::clamp(viewport_width * 22 / 100, 10, 24);
    columns = std::clamp(columns, 8, max_columns_fit);
    const float aspect = static_cast<float>(image->height) / static_cast<float>(image->width);
    // Keep the same cell-ratio behavior as graphics_layout: text rows are
    // approximately half of pixel rows.
    constexpr float kCellRowScale = 0.5f;
    int rows = std::max(4, static_cast<int>(aspect * static_cast<float>(columns) * kCellRowScale + 0.5f));

    const int max_rows = std::max(4, viewport_height - 2);
    if (rows > max_rows) {
        rows = max_rows;
        const float denom = std::max(0.01f, aspect * kCellRowScale);
        const int adjusted_columns = static_cast<int>(static_cast<float>(rows) / denom + 0.5f);
        columns = std::clamp(adjusted_columns, 8, max_columns_fit);
    }

    const int x = viewport_x + std::max(0, viewport_width - columns - 1);
    const int y = viewport_y;

    return GraphicsDrawCommand{
        backend,
        -1000000,
        -1,
        x,
        y,
        columns,
        rows,
        image,
    };
}

} // namespace

UIManager::UIManager(AppState& state, ClientConfig& cfg)
    : state_(state), cfg_(cfg) {
    // Initialize user list panel config from persisted settings
    user_list_config_.width = cfg.ui.user_list_width;
    user_list_config_.collapsed = cfg.ui.user_list_collapsed;
    initialize_clipboard_backend();
    configure_terminal_graphics(parse_terminal_graphics_mode(cfg_.preview.terminal_graphics));
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
            if (cfg_.ui.copy_selection_on_release) {
                copy_message_selection_to_clipboard();
            }
            auto sel = mouse_tracker_.selection_region();
            has_persistent_text_selection_ = (sel.width > 1 || sel.height > 1);
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
    if (!is_right_click && !mouse_tracker_.is_over_message_area()) {
        has_persistent_text_selection_ = false;
    }

    // Check tab bar clicks
    int tab_idx = get_tab_index_at_position(mouse_x, mouse_y);
    if (tab_idx >= 0) {
        // Channel switch via mouse
        auto channels = state_.channel_list();
        if (tab_idx < static_cast<int>(channels.size())) {
            has_persistent_text_selection_ = false;
            mouse_tracker_.end_selection();
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
            int line_y = mouse_y - mouse_tracker_.message_region().y;
            auto visible = current_visible_hits(
                state_, mouse_tracker_, user_list_config_, cfg_, screen_.dimx());
            if (line_y >= 0 && line_y < static_cast<int>(visible.size())) {
                if (visible[line_y].url) {
                    if (display_inline_image_from_url(screen_, *visible[line_y].url)) {
                        return;
                    }
                    open_url(*visible[line_y].url);
                    return;
                }

                auto ch = state_.active_channel();
                if (ch) {
                    auto ch_state = state_.channel_snapshot(*ch);
                    int message_index = visible[line_y].message_index;
                    if (message_index >= 0 &&
                        message_index < static_cast<int>(ch_state.messages.size())) {
                        const auto& msg = ch_state.messages[message_index];
                        if (visible[line_y].has_inline_image &&
                            msg.inline_image &&
                            display_inline_image(screen_, *msg.inline_image, msg.content)) {
                            return;
                        }
                        if (auto url = find_url_in_text(msg.content)) {
                            if (display_inline_image_from_url(screen_, *url)) {
                                return;
                            }
                            open_url(*url);
                            return;
                        }
                    }
                }
            }
        } else {
            int line_y = mouse_y - mouse_tracker_.message_region().y;
            auto visible = current_visible_hits(
                state_, mouse_tracker_, user_list_config_, cfg_, screen_.dimx());
            auto ch = state_.active_channel();
            if (ch && line_y >= 0 && line_y < static_cast<int>(visible.size())) {
                auto ch_state = state_.channel_snapshot(*ch);
                const int message_index = visible[line_y].message_index;
                if (message_index >= 0 &&
                    message_index < static_cast<int>(ch_state.messages.size()) &&
                    visible[line_y].has_inline_image &&
                    ch_state.messages[message_index].inline_image) {
                    remember_selected_image(*ch, message_index);
                }
            }
            has_persistent_text_selection_ = false;
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
        auto visible = current_visible_hits(
            state_, mouse_tracker_, user_list_config_, cfg_, screen_.dimx());
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
        auto visible = current_visible_hits(
            state_, mouse_tracker_, user_list_config_, cfg_, screen_.dimx());
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
        const auto& tab = tab_positions_[i];
        if (rel_x >= tab.x && rel_x < tab.x + tab.width) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

std::optional<std::string> UIManager::get_user_at_position(int mouse_x, int mouse_y) const {
    (void)mouse_x;  // Not used currently, but may be needed for future hit testing
    
    // Check each user position
    for (const auto& user : user_positions_) {
        if (mouse_x >= user.x && mouse_x < user.x + user.width &&
            mouse_y >= user.y && mouse_y < user.y + user.height) {
            return user.user_id;
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
    if (cfg_.ui.copy_selection_on_release) {
        copy_message_selection_to_clipboard();
    }
    auto sel = mouse_tracker_.selection_region();
    has_persistent_text_selection_ = (sel.width > 1 || sel.height > 1);
    mouse_tracker_.end_selection();
}

void UIManager::copy_message_selection_to_clipboard() {
    auto ch = state_.active_channel();
    if (!ch) {
        return;
    }

    const auto msg_region = mouse_tracker_.message_region();
    const auto sel = mouse_tracker_.selection_region();
    if (sel.height <= 0 || msg_region.height <= 0 || msg_region.width <= 0) {
        return;
    }
    // Ignore plain click (no drag): avoid copying accidental empty/whitespace selection.
    if (mouse_tracker_.selection_start_x() == mouse_tracker_.selection_end_x() &&
        mouse_tracker_.selection_start_y() == mouse_tracker_.selection_end_y()) {
        return;
    }

    auto ch_state = state_.channel_snapshot(*ch);
    const int visible_rows = msg_region.height;
    const int message_width = effective_message_width(
        mouse_tracker_, user_list_config_, screen_.dimx());
    auto visible = collect_visible_layout_hits(
        ch_state, cfg_.ui.timestamp_format, visible_rows, message_width);
    if (visible.empty()) {
        return;
    }

    const int start_line_raw = mouse_tracker_.selection_start_y() - msg_region.y;
    const int end_line_raw = mouse_tracker_.selection_end_y() - msg_region.y;
    const int start_col_raw = mouse_tracker_.selection_start_x() - msg_region.x;
    const int end_col_raw = mouse_tracker_.selection_end_x() - msg_region.x;

    int top_line = start_line_raw;
    int top_col = start_col_raw;
    int bottom_line = end_line_raw;
    int bottom_col = end_col_raw;
    if (start_line_raw > end_line_raw ||
        (start_line_raw == end_line_raw && start_col_raw > end_col_raw)) {
        top_line = end_line_raw;
        top_col = end_col_raw;
        bottom_line = start_line_raw;
        bottom_col = start_col_raw;
    }

    top_line = std::clamp(top_line, 0, static_cast<int>(visible.size()) - 1);
    bottom_line = std::clamp(bottom_line, top_line, static_cast<int>(visible.size()) - 1);
    top_col = std::clamp(top_col, 0, message_width);
    bottom_col = std::clamp(bottom_col, 0, message_width);

    std::vector<std::string> selected_lines;
    selected_lines.reserve(static_cast<size_t>(bottom_line - top_line + 1));
    bool has_selected_characters = false;
    for (int i = top_line; i <= bottom_line; ++i) {
        const std::string& line = visible[static_cast<size_t>(i)].plain_text;
        const int line_cols = visible_width(line);
        int from = (i == top_line) ? top_col : 0;
        int to = (i == bottom_line) ? (bottom_col + 1) : line_cols;
        from = std::clamp(from, 0, line_cols);
        to = std::clamp(to, from, line_cols);
        std::string slice = substring_by_display_columns(line, from, to);
        if (!slice.empty()) {
            const bool has_non_space = std::any_of(slice.begin(), slice.end(), [](unsigned char c) {
                return !std::isspace(c);
            });
            if (has_non_space) {
                has_selected_characters = true;
            }
        }
        selected_lines.push_back(std::move(slice));
    }

    std::string selected;
    for (size_t i = 0; i < selected_lines.size(); ++i) {
        if (i > 0) {
            selected.push_back('\n');
        }
        selected += selected_lines[i];
    }

    auto has_non_space_text = [](const std::string& text) {
        return std::any_of(text.begin(), text.end(), [](unsigned char c) {
            return !std::isspace(c);
        });
    };

    // Fallback to line-level copy if character slicing ends up empty.
    if (!has_selected_characters || !has_non_space_text(selected)) {
        selected.clear();
        for (int i = top_line; i <= bottom_line; ++i) {
            if (!selected.empty()) {
                selected.push_back('\n');
            }
            selected += visible[static_cast<size_t>(i)].plain_text;
        }
    }

    if (has_non_space_text(selected)) {
        copy_to_clipboard(selected);
        show_toast("Copied to clipboard");
    }
}

void UIManager::copy_selection_to_clipboard() {
    if (mouse_tracker_.is_selecting() || has_persistent_text_selection_) {
        copy_message_selection_to_clipboard();
        return;
    }
    // If there is no message selection, copy the input line contents
    std::string text = input_line_.text();
    if (!text.empty()) {
        copy_to_clipboard(text);
    }
}

void UIManager::notify() {
    screen_.PostEvent(Event::Custom);
}

void UIManager::show_toast(std::string text, std::chrono::milliseconds duration) {
    if (text.empty()) {
        return;
    }
    toast_text_ = std::move(text);
    toast_until_ = std::chrono::steady_clock::now() + duration;
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

void UIManager::remember_selected_image(const std::string& channel_id, int message_index) {
    selected_image_indices_[channel_id] = message_index;
}

std::optional<int> UIManager::selected_image_index(const std::string& channel_id,
                                                   const ChannelState& state) const {
    auto it = selected_image_indices_.find(channel_id);
    if (it != selected_image_indices_.end()) {
        int index = it->second;
        if (index >= 0 &&
            index < static_cast<int>(state.messages.size()) &&
            state.messages[static_cast<size_t>(index)].inline_image) {
            return index;
        }
    }

    for (int i = static_cast<int>(state.messages.size()) - 1; i >= 0; --i) {
        if (state.messages[static_cast<size_t>(i)].inline_image) {
            return i;
        }
    }
    return std::nullopt;
}

Element UIManager::build_main_content(const std::string& active_ch, int msg_rows, int term_cols) {
    // Ensure channel users are populated (from online users if not already set)
    if (!active_ch.empty()) {
        state_.ensure_channel_users_from_online(active_ch);
    }

    const bool show_user_list = !user_list_config_.collapsed && !is_server_channel(active_ch);

    int user_panel_width = 0;
    int msg_width = term_cols;
    if (show_user_list) {
        user_panel_width = get_panel_width(user_list_config_, term_cols);
        msg_width = std::max(1, term_cols - user_panel_width - 1);
    }

    ChannelState ch_state;
    if (!active_ch.empty()) {
        ch_state = state_.channel_snapshot(active_ch);
    }

    mouse_tracker_.set_message_region({0, mouse_tracker_.message_region().y, msg_width, msg_rows});

    int selected_start_row = -1;
    int selected_start_col = -1;
    int selected_end_row = -1;
    int selected_end_col = -1;
    if (mouse_tracker_.is_selecting() || has_persistent_text_selection_) {
        const auto msg_region = mouse_tracker_.message_region();
        int start_row = mouse_tracker_.selection_start_y() - msg_region.y;
        int end_row = mouse_tracker_.selection_end_y() - msg_region.y;
        int start_col = mouse_tracker_.selection_start_x() - msg_region.x;
        int end_col = mouse_tracker_.selection_end_x() - msg_region.x;

        if (start_row > end_row || (start_row == end_row && start_col > end_col)) {
            std::swap(start_row, end_row);
            std::swap(start_col, end_col);
        }

        selected_start_row = std::clamp(start_row, 0, std::max(0, msg_rows - 1));
        selected_end_row = std::clamp(end_row, selected_start_row, std::max(0, msg_rows - 1));
        selected_start_col = std::max(0, start_col);
        selected_end_col = std::max(selected_start_col, end_col);
    }

    Element msg_el = active_ch.empty()
        ? text("") | flex
        : render_messages(ch_state,
                          cfg_.ui.timestamp_format,
                          msg_rows,
                          msg_width,
                          selected_start_row,
                          selected_start_col,
                          selected_end_row,
                          selected_end_col);

    pending_graphics_frame_.viewport_x = mouse_tracker_.message_region().x;
    pending_graphics_frame_.viewport_y = mouse_tracker_.message_region().y;
    pending_graphics_frame_.viewport_width = msg_width;
    pending_graphics_frame_.viewport_height = msg_rows;
    pending_graphics_frame_.commands = active_ch.empty()
        ? std::vector<GraphicsDrawCommand>{}
        : collect_visible_draw_commands(ch_state,
                                        cfg_.ui.timestamp_format,
                                        msg_rows,
                                        msg_width,
                                        mouse_tracker_.message_region().x,
                                        mouse_tracker_.message_region().y);
    if (is_server_channel(active_ch)) {
        auto background = make_server_background_command(
            pending_graphics_frame_.viewport_x,
            pending_graphics_frame_.viewport_y,
            pending_graphics_frame_.viewport_width,
            pending_graphics_frame_.viewport_height);
        if (background) {
            pending_graphics_frame_.commands.insert(
                pending_graphics_frame_.commands.begin(),
                std::move(*background));
        }
    }
    
    // If user list is collapsed, just return the message view
    if (!show_user_list) {
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
    int user_list_x = msg_width + 1;
    mouse_tracker_.set_user_list_region({user_list_x, mouse_tracker_.message_region().y, user_panel_width, msg_rows});
    mouse_tracker_.set_panel_divider_region({user_list_x - 1, mouse_tracker_.message_region().y, 1, msg_rows});
    panel_divider_x_ = user_list_x - 1;
    
    // Render user list panel with position tracking
    auto user_list_el = render_user_list_panel(users, voice_sec, user_list_config_, 
                                                  state_.local_user_id(), user_positions_, 
                                                  panel_divider_x_, user_list_x,
                                                  mouse_tracker_.message_region().y);
    
    // Combine message view and user list panel horizontally.
    // HEIGHT LESS_THAN keeps a long user list from overflowing the content area.
    return hbox({
        msg_el | flex,
        separator(),
        user_list_el | size(WIDTH, EQUAL, user_panel_width)
                     | size(HEIGHT, LESS_THAN, msg_rows + 1),
    });
}

Element UIManager::build_document(int term_rows) {
    int term_cols = screen_.dimx() > 0 ? screen_.dimx() : 80;
    
    // Update region tracking for mouse hit testing
    mouse_tracker_.set_tab_bar_region({0, 0, term_cols, kTabBarHeight});
    
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
    
    mouse_tracker_.set_message_region({0, msg_y, term_cols, msg_rows});
    mouse_tracker_.set_status_bar_region({0, status_y, term_cols, kStatusBarHeight});
    mouse_tracker_.set_input_region({0, input_y, term_cols, input_lines});
    
    // Panels
    auto channels  = state_.channel_list();
    auto active_ch = state_.active_channel().value_or("");

    if (active_ch != last_active_channel_) {
        has_persistent_text_selection_ = false;
        mouse_tracker_.end_selection();
        last_active_channel_ = active_ch;
    }

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
    si.connecting     = state_.connecting();
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

    auto base_document = vbox({
        tab_el,
        separator(),
        main_content | flex,
        separator(),
        status_el,
        separator(),
        input_el,
    }) | bgcolor(palette::bg());

    Element document = std::move(base_document);

    const auto now = std::chrono::steady_clock::now();
    if (!toast_text_.empty() && now < toast_until_) {
        auto toast_overlay = vbox({
            hbox({
                filler(),
                text(" " + toast_text_ + " ")
                    | color(palette::fg())
                    | bgcolor(palette::bg_highlight()),
            }),
            filler(),
        });
        document = dbox({
            std::move(document),
            std::move(toast_overlay),
        });
    }

    if (!quit_confirm_visible_) {
        quit_confirm_yes_button_ = {};
        quit_confirm_no_button_ = {};
        return document;
    }

    // Keep modal focused and uncluttered: hide inline graphics while confirm is open.
    pending_graphics_frame_.commands.clear();

    const bool is_finnish = (cfg_.ui.language == "fi");
    const std::string title = is_finnish ? "Poistumisvarmistus" : "Quit Confirmation";
    const std::string question = is_finnish
        ? "Haluatko varmasti poistua?"
        : "Are you sure you want to quit?";
    const std::string yes_label = is_finnish ? " Kyllä " : " Yes ";
    const std::string no_label = is_finnish ? " Peruuta " : " Cancel ";

    const int box_width = 44;
    const int box_height = 7;
    const int box_x = std::max(0, (term_cols - box_width) / 2);
    const int box_y = std::max(0, (term_rows - box_height) / 2);
    const int inner_width = std::max(1, box_width - 2);

    const int total_button_width = static_cast<int>(yes_label.size() + 3 + no_label.size());
    const int button_start_x = box_x + 1 + std::max(0, (inner_width - total_button_width) / 2);
    const int button_y = box_y + 4;
    quit_confirm_yes_button_ = {button_start_x, button_y, static_cast<int>(yes_label.size()), 1};
    quit_confirm_no_button_ = {button_start_x + static_cast<int>(yes_label.size()) + 3, button_y, static_cast<int>(no_label.size()), 1};

    Elements scrim_rows;
    scrim_rows.reserve(static_cast<size_t>(std::max(1, term_rows)));
    for (int row = 0; row < std::max(1, term_rows); ++row) {
        scrim_rows.push_back(text(std::string(static_cast<size_t>(std::max(1, term_cols)), ' '))
                             | bgcolor(palette::bg_dark()));
    }
    auto scrim_overlay = vbox(std::move(scrim_rows));

    auto confirm_overlay = vbox({
        filler(),
        hbox({
            filler(),
            vbox({
                hbox({
                    text(title) | bold | color(palette::blue()),
                    filler(),
                }) | bgcolor(palette::bg()),
                separator(),
                hbox({
                    text(question) | color(palette::fg()),
                    filler(),
                }) | bgcolor(palette::bg()),
                hbox({ filler() }) | bgcolor(palette::bg()),
                hbox({
                    filler(),
                    text(yes_label) | bold | color(palette::bg()) | bgcolor(palette::blue()),
                    text("   ") | bgcolor(palette::bg()),
                    text(no_label) | color(palette::fg()) | bgcolor(palette::bg_highlight()),
                    filler(),
                }) | bgcolor(palette::bg()),
                hbox({ filler() }) | bgcolor(palette::bg()),
            }) | border | bgcolor(palette::bg()) | size(WIDTH, EQUAL, box_width) | size(HEIGHT, EQUAL, box_height),
            filler(),
        }),
        filler(),
    });

    return dbox({
        std::move(document),
        std::move(scrim_overlay),
        std::move(confirm_overlay),
    });
}

void UIManager::run(SubmitFn on_submit,
                    std::function<void()> on_quit,
                    std::function<void(int)> on_channel_switch,
                    ChannelCycleFn on_channel_cycle,
                    PttToggleFn on_ptt_toggle,
                    OpenSettingsFn on_open_settings) {
#ifndef _WIN32
    // Re-assert swallow handlers at UI loop boundary.
    install_interrupt_handlers();
    ScopedDisableTtyInterruptSignals scoped_tty_interrupt_disable;
#endif
    screen_.ForceHandleCtrlC(false);

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
        auto document = build_document(rows);
        graphics_compositor_.commit(pending_graphics_frame_);
        return document;
    });

    auto event_handler = CatchEvent(renderer, [&](Event event) -> bool {
        if (quit_confirm_visible_) {
            if (event == Event::Custom) {
                return true;
            }
            if (event.is_mouse()) {
                Event& evt = const_cast<Event&>(event);
                auto& mouse = evt.mouse();
                if (mouse.button == Mouse::Left && mouse.motion == Mouse::Pressed) {
                    if (quit_confirm_yes_button_.contains(mouse.x, mouse.y)) {
                        if (on_quit) on_quit();
                        screen_.ExitLoopClosure()();
                        return true;
                    }
                    if (quit_confirm_no_button_.contains(mouse.x, mouse.y)) {
                        quit_confirm_visible_ = false;
                        return true;
                    }
                }
                return true;
            }
            if (event == Event::Escape || event == Event::Character("n") || event == Event::Character("N")) {
                quit_confirm_visible_ = false;
                return true;
            }
            if (event == Event::Return || event == Event::Character("y") || event == Event::Character("Y")) {
                if (on_quit) on_quit();
                screen_.ExitLoopClosure()();
                return true;
            }
            // Modal open: consume all other events.
            return true;
        }

        // Handle mouse events first
        if (event.is_mouse()) {
            return handle_mouse_event(event);
        }
        
        if (event == Event::Custom) {
            // Posted by notify() — just triggers a redraw
            return true;
        }
        if (auto pasted = extract_bracketed_paste(event.input())) {
            tab_completer_.reset();
            input_line_.insert_text(*pasted);
            return true;
        }
        if (is_shift_insert_paste(event.input())) {
            if (auto clipboard = read_from_clipboard()) {
                tab_completer_.reset();
                input_line_.insert_text(*clipboard);
            }
            return true;
        }
        if (event.input() == "\x16") {
            if (auto clipboard = read_from_clipboard()) {
                tab_completer_.reset();
                input_line_.insert_text(*clipboard);
            }
            return true;
        }
        if (event == Event::CtrlC || event.input() == "\x03") {
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
            quit_confirm_visible_ = true;
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
