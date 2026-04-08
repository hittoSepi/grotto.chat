#include "ui/settings_screen.hpp"
#include "ui/color_scheme.hpp"
#include "ui/key_name.hpp"
#include "ui/modal_overlay.hpp"
#include "i18n/strings.hpp"
#include "voice/audio_device.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>
#include <toml.hpp>

#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <optional>

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
#endif

// Available themes
const std::vector<std::string> kThemes = {
    "tokyo-night",
    "dark",
    "light",
    "dracula",
    "nord",
    "solarized-dark",
    "solarized-light"
};

// Default config values
constexpr bool kDefaultAutoReconnect = true;
constexpr int kDefaultReconnectDelay = 5;
constexpr int kDefaultTimeout = 30;
constexpr bool kDefaultTlsVerify = true;
constexpr bool kDefaultDesktopNotifications = true;
constexpr bool kDefaultSoundAlerts = true;
constexpr bool kDefaultNotifyMention = true;
constexpr bool kDefaultNotifyDM = true;
constexpr bool kDefaultShareTypingIndicators = true;
constexpr bool kDefaultShareReadReceipts = true;
constexpr bool kDefaultShowTimestamps = true;
constexpr bool kDefaultShowUserColors = true;
constexpr int kDefaultFontScale = 100;
constexpr int kDefaultMaxMessages = 1000;

// Category labels
std::string category_label(SettingsCategory cat) {
    switch (cat) {
        case SettingsCategory::General: return i18n::tr(i18n::I18nKey::CATEGORY_GENERAL);
        case SettingsCategory::Appearance: return i18n::tr(i18n::I18nKey::CATEGORY_APPEARANCE);
        case SettingsCategory::Voice: return i18n::tr(i18n::I18nKey::CATEGORY_VOICE);
        case SettingsCategory::Connection: return i18n::tr(i18n::I18nKey::CATEGORY_CONNECTION);
        case SettingsCategory::Notifications: return i18n::tr(i18n::I18nKey::CATEGORY_NOTIFICATIONS);
        case SettingsCategory::Privacy: return i18n::tr(i18n::I18nKey::CATEGORY_PRIVACY);
        case SettingsCategory::Account: return i18n::tr(i18n::I18nKey::CATEGORY_ACCOUNT);
    }
    return "Unknown";
}

int terminal_graphics_to_index(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "off") {
        return 1;
    }
    if (value == "viewer-only") {
        return 2;
    }
    return 0;
}

std::string terminal_graphics_from_index(int index) {
    if (index == 1) {
        return "off";
    }
    if (index == 2) {
        return "viewer-only";
    }
    return "auto";
}

int language_to_index(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value == "en" ? 1 : 0;
}

std::string language_from_index(int index) {
    return index == 1 ? "en" : "fi";
}

int theme_to_index(const std::vector<std::string>& options, const std::string& value) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == value) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

std::string theme_from_index(const std::vector<std::string>& options, int index) {
    if (options.empty()) {
        return "tokyo-night";
    }
    if (index < 0 || index >= static_cast<int>(options.size())) {
        return options.front();
    }
    return options[static_cast<size_t>(index)];
}

int voice_mode_to_index(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value == "vox" ? 1 : 0;
}

std::string voice_mode_from_index(int index) {
    return index == 1 ? "vox" : "ptt";
}

int noise_suppression_level_to_index(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "low") {
        return 0;
    }
    if (value == "high") {
        return 2;
    }
    if (value == "very_high") {
        return 3;
    }
    return 1;
}

std::string noise_suppression_level_from_index(int index) {
    switch (index) {
        case 0: return "low";
        case 2: return "high";
        case 3: return "very_high";
        default: return "moderate";
    }
}

int find_device_index(const std::vector<std::string>& values,
                      const std::string& selected_value) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == selected_value) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

} // anonymous namespace

const std::vector<std::string>& available_themes() {
    return kThemes;
}

void apply_theme(const std::string& theme_name) {
    // Theme is applied through color_scheme.hpp - colors are currently static
    // In a full implementation, this would update palette colors dynamically
    spdlog::info("Theme changed to: {}", theme_name);
}

SettingsScreen::SettingsScreen() = default;

void SettingsScreen::set_active_category(SettingsCategory category) {
    active_category_ = category;
    active_category_index_ = static_cast<int>(category);
}

SettingsResult SettingsScreen::show(ClientConfig& cfg,
                                    ftxui::ScreenInteractive& screen,
                                    const std::string& public_key_hex,
                                    ThemeChangeFn on_theme_change) {
    saved_ = false;
    cancelled_ = false;
    logout_ = false;
    original_cfg_ = cfg;
    public_key_hex_ = public_key_hex;
    on_theme_change_ = on_theme_change;
    config_path_ = cfg.config_dir / "config.toml";

    auto exit_closure = screen.ExitLoopClosure();
    exit_closure_ = exit_closure;
    
    load_settings_from_config(cfg);
    build_ui();
    
#ifndef _WIN32
    install_interrupt_handlers();
#endif
    screen.ForceHandleCtrlC(false);
    
    // Main renderer
    auto renderer = Renderer(container_, [this] {
        // Sidebar
        auto sidebar = vbox({
            text(" " + i18n::tr(i18n::I18nKey::SETTINGS_TITLE) + " ") | bold | color(palette::blue()),
            separator(),
            sidebar_container_->Render(),
        }) | border | size(WIDTH, LESS_THAN, 20);
        
        // Content area based on active category
        Element content;
        switch (active_category_) {
            case SettingsCategory::General:
                content = render_general();
                break;
            case SettingsCategory::Appearance:
                content = render_appearance();
                break;
            case SettingsCategory::Voice:
                content = render_voice();
                break;
            case SettingsCategory::Connection:
                content = render_connection();
                break;
            case SettingsCategory::Notifications:
                content = render_notifications();
                break;
            case SettingsCategory::Privacy:
                content = render_privacy();
                break;
            case SettingsCategory::Account:
                content = render_account();
                break;
        }
        
        // Action buttons at bottom
        auto actions = hbox({
            save_button_->Render(),
            text(" "),
            cancel_button_->Render(),
            filler(),
            reset_button_->Render(),
        });
        
        auto main_content = vbox({
            text(" " + category_label(active_category_) + " ") | bold | color(palette::blue()),
            separator(),
            content | flex,
            separator(),
            actions,
        }) | border | flex;
        
        Element root = hbox({
            sidebar,
            main_content,
        }) | bgcolor(palette::bg()) | color(palette::fg());

        if (voice_key_capture_visible_) {
            auto modal = build_modal_box(
                i18n::tr(i18n::I18nKey::VOICE_PTT_CAPTURE_TITLE),
                {
                    hbox({ text(i18n::tr(i18n::I18nKey::VOICE_PTT_CAPTURE_HINT)), filler() }),
                    hbox({ text(i18n::tr(i18n::I18nKey::VOICE_PTT_CAPTURE_CANCEL_HINT)), filler() }),
                },
                44);
            root = overlay_centered_modal(std::move(root), std::move(modal));
        }
        return root;
    });
    
    // Event handler
    auto component = CatchEvent(renderer, [this, exit_closure, &cfg](Event event) -> bool {
        if (voice_key_capture_visible_) {
            if (event == Event::Escape) {
                voice_key_capture_visible_ = false;
                return true;
            }
            if (event.is_mouse()) {
                return true;
            }
            if (auto captured = key_name_from_event(event)) {
                voice_ptt_key_ = *captured;
                voice_key_capture_visible_ = false;
                return true;
            }
            return true;
        }

        if (event == Event::CtrlC || event.input() == "\x03" || event.input() == "\x04") {
            return true;
        }
        if (event == Event::Escape) {
            cancelled_ = true;
            exit_closure();
            return true;
        }
        if (event == Event::Character("q") || event == Event::Character("Q")) {
            cancelled_ = true;
            exit_closure();
            return true;
        }
        // F1-F7 to switch categories
        if (event == Event::F1) {
            set_active_category(SettingsCategory::General);
            return true;
        }
        if (event == Event::F2) {
            set_active_category(SettingsCategory::Appearance);
            return true;
        }
        if (event == Event::F3) {
            set_active_category(SettingsCategory::Voice);
            return true;
        }
        if (event == Event::F4) {
            set_active_category(SettingsCategory::Connection);
            return true;
        }
        if (event == Event::F5) {
            set_active_category(SettingsCategory::Notifications);
            return true;
        }
        if (event == Event::F6) {
            set_active_category(SettingsCategory::Privacy);
            return true;
        }
        if (event == Event::F7) {
            set_active_category(SettingsCategory::Account);
            return true;
        }
        return false;
    });
    
    screen.Loop(component);
    exit_closure_ = {};
    
    if (logout_) {
        return SettingsResult::Logout;
    }
    if (cancelled_) {
        cfg = original_cfg_; // Restore original config
        return SettingsResult::Cancelled;
    }
    if (saved_) {
        save_settings_to_config(cfg);
        return SettingsResult::Saved;
    }
    return SettingsResult::Cancelled;
}

void SettingsScreen::build_ui() {
    // Sidebar category buttons
    sidebar_container_ = Container::Vertical({
        Button(" " + i18n::tr(i18n::I18nKey::CATEGORY_GENERAL) + " ", [this] { set_active_category(SettingsCategory::General); }),
        Button(" " + i18n::tr(i18n::I18nKey::CATEGORY_APPEARANCE) + " ", [this] { set_active_category(SettingsCategory::Appearance); }),
        Button(" " + i18n::tr(i18n::I18nKey::CATEGORY_VOICE) + " ", [this] { set_active_category(SettingsCategory::Voice); }),
        Button(" " + i18n::tr(i18n::I18nKey::CATEGORY_CONNECTION) + " ", [this] { set_active_category(SettingsCategory::Connection); }),
        Button(" " + i18n::tr(i18n::I18nKey::CATEGORY_NOTIFICATIONS) + " ", [this] { set_active_category(SettingsCategory::Notifications); }),
        Button(" " + i18n::tr(i18n::I18nKey::CATEGORY_PRIVACY) + " ", [this] { set_active_category(SettingsCategory::Privacy); }),
        Button(" " + i18n::tr(i18n::I18nKey::CATEGORY_ACCOUNT) + " ", [this] { set_active_category(SettingsCategory::Account); }),
    });
    
    // Input components
    theme_options_ = available_themes();
    if (theme_options_.empty()) {
        theme_options_.push_back("tokyo-night");
    }
    theme_selected_ = theme_to_index(theme_options_, theme_);
    theme_toggle_ = Toggle(&theme_options_, &theme_selected_);
    timestamp_format_input_ = Input(&timestamp_format_, "%H:%M");
    max_messages_input_ = Input(&max_messages_, "1000");
    image_columns_input_ = Input(&image_columns_, "40");
    image_rows_input_ = Input(&image_rows_, "16");
    terminal_graphics_selected_ = terminal_graphics_to_index(terminal_graphics_);
    terminal_graphics_toggle_ = Toggle(&terminal_graphics_options_, &terminal_graphics_selected_);
    voice_mode_options_ = {
        i18n::tr(i18n::I18nKey::VOICE_MODE_PTT),
        i18n::tr(i18n::I18nKey::VOICE_MODE_VOX),
    };
    voice_noise_suppression_level_options_ = {
        i18n::tr(i18n::I18nKey::VOICE_NS_LEVEL_LOW),
        i18n::tr(i18n::I18nKey::VOICE_NS_LEVEL_MODERATE),
        i18n::tr(i18n::I18nKey::VOICE_NS_LEVEL_HIGH),
        i18n::tr(i18n::I18nKey::VOICE_NS_LEVEL_VERY_HIGH),
    };
    voice_input_device_dropdown_ = Dropdown(&voice_input_device_options_, &voice_input_device_selected_);
    voice_output_device_dropdown_ = Dropdown(&voice_output_device_options_, &voice_output_device_selected_);
    voice_mode_dropdown_ = Dropdown(&voice_mode_options_, &voice_mode_selected_);
    voice_noise_suppression_cb_ = Checkbox(i18n::tr(i18n::I18nKey::VOICE_NOISE_SUPPRESSION_LABEL), &voice_noise_suppression_enabled_);
    voice_noise_suppression_level_dropdown_ = Dropdown(&voice_noise_suppression_level_options_, &voice_noise_suppression_level_selected_);
    voice_capture_key_button_ = Button(i18n::tr(i18n::I18nKey::VOICE_SET_HOTKEY_BUTTON), [this] {
        voice_key_capture_visible_ = true;
    });
    voice_vad_threshold_slider_ = Slider("", &voice_vad_threshold_percent_, 0, 100, 1);
    voice_jitter_buffer_slider_ = Slider("", &voice_jitter_buffer_frames_, 2, 10, 1);
    voice_limiter_cb_ = Checkbox(i18n::tr(i18n::I18nKey::VOICE_LIMITER_LABEL), &voice_limiter_enabled_);
    voice_limiter_threshold_slider_ = Slider("", &voice_limiter_threshold_percent_, 20, 99, 1);
    voice_input_volume_slider_ = Slider("", &voice_input_volume_value_, 0, 200, 1);
    voice_output_volume_slider_ = Slider("", &voice_output_volume_value_, 0, 200, 1);
    reconnect_delay_input_ = Input(&reconnect_delay_sec_text_, "5");
    timeout_input_ = Input(&connection_timeout_sec_text_, "30");
    cert_pin_input_ = Input(&tls_cert_pin_, "");
    keywords_input_ = Input(&mention_keywords_, "");
    nickname_input_ = Input(&nickname_, "");
    language_selected_ = language_to_index(language_);
    language_toggle_ = Toggle(&language_options_, &language_selected_);

    // Checkbox components (created once, not per-render)
    copy_selection_on_release_cb_ = Checkbox(i18n::tr(i18n::I18nKey::COPY_ON_RELEASE), &copy_selection_on_release_);
    inline_images_cb_ = Checkbox(i18n::tr(i18n::I18nKey::INLINE_IMAGES), &inline_images_);
    show_timestamps_cb_ = Checkbox(i18n::tr(i18n::I18nKey::SHOW_TIMESTAMPS), &show_timestamps_);
    show_user_colors_cb_ = Checkbox(i18n::tr(i18n::I18nKey::COLORIZE_USERNAMES), &show_user_colors_);
    auto_reconnect_cb_ = Checkbox(i18n::tr(i18n::I18nKey::AUTO_RECONNECT), &auto_reconnect_);
    tls_verify_cb_ = Checkbox(i18n::tr(i18n::I18nKey::VERIFY_TLS), &tls_verify_peer_);
    desktop_notif_cb_ = Checkbox(i18n::tr(i18n::I18nKey::DESKTOP_NOTIFICATIONS), &desktop_notifications_);
    sound_alerts_cb_ = Checkbox(i18n::tr(i18n::I18nKey::SOUND_ALERTS), &sound_alerts_);
    mention_cb_ = Checkbox(i18n::tr(i18n::I18nKey::NOTIFY_ON_MENTION), &notify_on_mention_);
    dm_cb_ = Checkbox(i18n::tr(i18n::I18nKey::NOTIFY_ON_DM), &notify_on_dm_);
    share_typing_indicators_cb_ = Checkbox(i18n::tr(i18n::I18nKey::SHARE_TYPING_INDICATORS), &share_typing_indicators_);
    share_read_receipts_cb_ = Checkbox(i18n::tr(i18n::I18nKey::SHARE_READ_RECEIPTS), &share_read_receipts_);

    // Account action buttons (created once, not per-render)
    export_button_persistent_ = Button(i18n::tr(i18n::I18nKey::BUTTON_EXPORT_SETTINGS), [this] { export_settings(); });
    import_button_persistent_ = Button(i18n::tr(i18n::I18nKey::BUTTON_IMPORT_SETTINGS), [this] { import_settings(); });
    logout_button_persistent_ = Button(i18n::tr(i18n::I18nKey::BUTTON_LOGOUT), [this] {
        logout_ = true;
        if (exit_closure_) exit_closure_();
    });

    // Action buttons
    save_button_ = Button(i18n::tr(i18n::I18nKey::BUTTON_SAVE), [this] {
        saved_ = true;
        if (exit_closure_) exit_closure_();
    });
    cancel_button_ = Button(i18n::tr(i18n::I18nKey::BUTTON_CANCEL), [this] {
        cancelled_ = true;
        if (exit_closure_) exit_closure_();
    });
    reset_button_ = Button(i18n::tr(i18n::I18nKey::BUTTON_RESET_DEFAULTS), [this] {
        reset_to_defaults();
    });

    general_container_ = Container::Vertical({
        copy_selection_on_release_cb_,
        inline_images_cb_,
        image_columns_input_,
        image_rows_input_,
        terminal_graphics_toggle_,
    });

    appearance_container_ = Container::Vertical({
        theme_toggle_,
        timestamp_format_input_,
        max_messages_input_,
        language_toggle_,
        show_timestamps_cb_,
        show_user_colors_cb_,
    });

    voice_container_ = Container::Vertical({
        voice_input_device_dropdown_,
        voice_output_device_dropdown_,
        voice_mode_dropdown_,
        voice_noise_suppression_cb_,
        voice_noise_suppression_level_dropdown_,
        voice_capture_key_button_,
        voice_vad_threshold_slider_,
        voice_jitter_buffer_slider_,
        voice_limiter_cb_,
        voice_limiter_threshold_slider_,
        voice_input_volume_slider_,
        voice_output_volume_slider_,
    });

    connection_container_ = Container::Vertical({
        reconnect_delay_input_,
        timeout_input_,
        auto_reconnect_cb_,
        tls_verify_cb_,
        cert_pin_input_,
    });

    notifications_container_ = Container::Vertical({
        desktop_notif_cb_,
        sound_alerts_cb_,
        mention_cb_,
        dm_cb_,
        keywords_input_,
    });

    privacy_container_ = Container::Vertical({
        share_typing_indicators_cb_,
        share_read_receipts_cb_,
    });

    account_container_ = Container::Vertical({
        nickname_input_,
        export_button_persistent_,
        import_button_persistent_,
        logout_button_persistent_,
    });

    content_container_ = Container::Tab({
        general_container_,
        appearance_container_,
        voice_container_,
        connection_container_,
        notifications_container_,
        privacy_container_,
        account_container_,
    }, &active_category_index_);

    actions_container_ = Container::Horizontal({
        save_button_,
        cancel_button_,
        reset_button_,
    });

    container_ = Container::Horizontal({
        sidebar_container_,
        Container::Vertical({
            content_container_,
            actions_container_,
        }),
    });
}

Element SettingsScreen::render_appearance() {
    // Theme selection dropdown/toggle
    auto theme_row = hbox({
        text(i18n::tr(i18n::I18nKey::THEME_LABEL)) | color(palette::fg_dark()),
        theme_toggle_->Render() | border,
        text(" (" + std::to_string(theme_options_.size()) + i18n::tr(i18n::I18nKey::THEME_AVAILABLE) + ")" ) | color(palette::comment()),
    });
    
    // Font scale slider representation
    auto font_row = hbox({
        text(i18n::tr(i18n::I18nKey::FONT_SCALE_LABEL)) | color(palette::fg_dark()),
        text(std::to_string(font_scale_) + "%") | color(palette::cyan()),
    });
    
    // Display options (use persistent components)
    
    // Timestamp format
    auto format_row = hbox({
        text(i18n::tr(i18n::I18nKey::TIMESTAMP_FORMAT_LABEL)) | color(palette::fg_dark()),
        timestamp_format_input_->Render() | size(WIDTH, GREATER_THAN, 15) | border,
    });
    
    // Max messages
    auto max_msg_row = hbox({
        text(i18n::tr(i18n::I18nKey::MAX_MESSAGES_LABEL)) | color(palette::fg_dark()),
        max_messages_input_->Render() | size(WIDTH, GREATER_THAN, 10) | border,
    });
    
    return vbox({
        text(i18n::tr(i18n::I18nKey::THEME_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        theme_row,
        text(""),
        text(i18n::tr(i18n::I18nKey::DISPLAY_OPTIONS)) | bold | color(palette::blue()),
        separator(),
        show_timestamps_cb_->Render(),
        show_user_colors_cb_->Render(),
        format_row,
        max_msg_row,
        text(""),
        hbox({
            text(i18n::tr(i18n::I18nKey::LANGUAGE_LABEL)) | color(palette::fg_dark()),
            language_toggle_->Render() | border,
        }),
        text(""),
        text(i18n::tr(i18n::I18nKey::THEME_NOTE))
            | color(palette::comment()) | dim,
    });
}

Element SettingsScreen::render_general() {
    auto image_columns_row = hbox({
        text(i18n::tr(i18n::I18nKey::IMAGE_COLUMNS_LABEL)) | color(palette::fg_dark()),
        image_columns_input_->Render() | size(WIDTH, GREATER_THAN, 6) | border,
    });

    auto image_rows_row = hbox({
        text(i18n::tr(i18n::I18nKey::IMAGE_ROWS_LABEL)) | color(palette::fg_dark()),
        image_rows_input_->Render() | size(WIDTH, GREATER_THAN, 6) | border,
    });

    auto terminal_graphics_row = hbox({
        text(i18n::tr(i18n::I18nKey::TERMINAL_GRAPHICS_LABEL)) | color(palette::fg_dark()),
        terminal_graphics_toggle_->Render() | border,
    });

    return vbox({
        text(i18n::tr(i18n::I18nKey::CLIPBOARD_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        copy_selection_on_release_cb_->Render(),
        text(""),
        text(i18n::tr(i18n::I18nKey::PREVIEW_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        inline_images_cb_->Render(),
        image_columns_row,
        image_rows_row,
        terminal_graphics_row,
        text(i18n::tr(i18n::I18nKey::TERMINAL_GRAPHICS_HINT)) | color(palette::comment()) | dim,
    });
}

Element SettingsScreen::render_voice() {
    auto input_device_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_INPUT_DEVICE_LABEL)) | color(palette::fg_dark()),
        voice_input_device_dropdown_->Render() | border | flex,
    });

    auto output_device_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_OUTPUT_DEVICE_LABEL)) | color(palette::fg_dark()),
        voice_output_device_dropdown_->Render() | border | flex,
    });

    auto input_volume_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_INPUT_VOLUME_LABEL)) | color(palette::fg_dark()),
        voice_input_volume_slider_->Render() | flex,
        text(" " + std::to_string(voice_input_volume_value_) + "%") | color(palette::cyan()),
    });

    auto output_volume_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_OUTPUT_VOLUME_LABEL)) | color(palette::fg_dark()),
        voice_output_volume_slider_->Render() | flex,
        text(" " + std::to_string(voice_output_volume_value_) + "%") | color(palette::cyan()),
    });

    auto voice_mode_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_MODE_LABEL)) | color(palette::fg_dark()),
        voice_mode_dropdown_->Render() | border,
    });

    auto ptt_key_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_PTT_HOTKEY_LABEL)) | color(palette::fg_dark()),
        text(voice_ptt_key_) | bold | color(palette::cyan()),
        text("  "),
        voice_capture_key_button_->Render(),
    });

    auto noise_suppression_level_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_NOISE_SUPPRESSION_LEVEL_LABEL)) | color(palette::fg_dark()),
        voice_noise_suppression_level_dropdown_->Render() | border,
    });

    auto vad_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_VAD_THRESHOLD_LABEL)) | color(palette::fg_dark()),
        voice_vad_threshold_slider_->Render() | flex,
        text(" " + std::to_string(voice_vad_threshold_percent_) + "%") | color(palette::cyan()),
    });

    auto jitter_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_JITTER_BUFFER_LABEL)) | color(palette::fg_dark()),
        voice_jitter_buffer_slider_->Render() | flex,
        text(" " + std::to_string(voice_jitter_buffer_frames_) + " fr") | color(palette::cyan()),
    });

    auto limiter_threshold_row = hbox({
        text(i18n::tr(i18n::I18nKey::VOICE_LIMITER_THRESHOLD_LABEL)) | color(palette::fg_dark()),
        voice_limiter_threshold_slider_->Render() | flex,
        text(" " + std::to_string(voice_limiter_threshold_percent_) + "%") | color(palette::cyan()),
    });

    return vbox({
        text(i18n::tr(i18n::I18nKey::VOICE_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        input_device_row,
        output_device_row,
        text(""),
        input_volume_row,
        output_volume_row,
        text(""),
        voice_mode_row,
        voice_noise_suppression_cb_->Render(),
        noise_suppression_level_row,
        voice_limiter_cb_->Render(),
        limiter_threshold_row,
        ptt_key_row,
        vad_row,
        jitter_row,
        text(i18n::tr(i18n::I18nKey::VOICE_SETTINGS_HINT)) | color(palette::comment()) | dim,
    });
}

Element SettingsScreen::render_connection() {
    auto delay_row = hbox({
        text(i18n::tr(i18n::I18nKey::RECONNECT_DELAY_LABEL)) | color(palette::fg_dark()),
        reconnect_delay_input_->Render() | size(WIDTH, GREATER_THAN, 5) | border,
        text(i18n::tr(i18n::I18nKey::SECONDS)) | color(palette::comment()),
    });
    
    auto timeout_row = hbox({
        text(i18n::tr(i18n::I18nKey::CONNECTION_TIMEOUT_LABEL)) | color(palette::fg_dark()),
        timeout_input_->Render() | size(WIDTH, GREATER_THAN, 5) | border,
        text(i18n::tr(i18n::I18nKey::SECONDS)) | color(palette::comment()),
    });
    
    auto cert_pin_row = hbox({
        text(i18n::tr(i18n::I18nKey::CERT_PIN_LABEL)) | color(palette::fg_dark()),
        cert_pin_input_->Render() | size(WIDTH, GREATER_THAN, 40) | border,
    });
    
    return vbox({
        text(i18n::tr(i18n::I18nKey::RECONNECT_BEHAVIOR)) | bold | color(palette::blue()),
        separator(),
        auto_reconnect_cb_->Render(),
        delay_row,
        text(""),
        text(i18n::tr(i18n::I18nKey::TIMEOUT_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        timeout_row,
        text(""),
        text(i18n::tr(i18n::I18nKey::TLS_OPTIONS)) | bold | color(palette::blue()),
        separator(),
        tls_verify_cb_->Render(),
        cert_pin_row,
        text(""),
        text(i18n::tr(i18n::I18nKey::CONNECTION_NOTE))
            | color(palette::comment()) | dim,
    });
}

Element SettingsScreen::render_notifications() {
    auto keywords_row = hbox({
        text(i18n::tr(i18n::I18nKey::MENTION_KEYWORDS_LABEL)) | color(palette::fg_dark()),
        keywords_input_->Render() | size(WIDTH, GREATER_THAN, 30) | border,
    });
    
    return vbox({
        text(i18n::tr(i18n::I18nKey::NOTIFICATION_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        desktop_notif_cb_->Render(),
        sound_alerts_cb_->Render(),
        text(""),
        text(i18n::tr(i18n::I18nKey::MENTION_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        mention_cb_->Render(),
        dm_cb_->Render(),
        keywords_row,
        text(i18n::tr(i18n::I18nKey::MENTION_KEYWORDS_HINT)) | color(palette::comment()) | dim,
    });
}

Element SettingsScreen::render_privacy() {
    return vbox({
        share_typing_indicators_cb_->Render(),
        share_read_receipts_cb_->Render(),
        text(""),
        text(i18n::tr(i18n::I18nKey::PRIVACY_NOTE)) | color(palette::comment()) | dim,
    });
}

Element SettingsScreen::render_account() {
    auto nickname_row = hbox({
        text(i18n::tr(i18n::I18nKey::NICKNAME_LABEL)) | color(palette::fg_dark()),
        nickname_input_->Render() | size(WIDTH, GREATER_THAN, 25) | border,
    });
    
    // Public key display (read-only)
    std::string pk_display = public_key_hex_.empty() ? i18n::tr(i18n::I18nKey::PUBLIC_KEY_NOT_AVAILABLE) : public_key_hex_;
    if (pk_display.size() > 50) {
        pk_display = pk_display.substr(0, 47) + "...";
    }
    
    auto pubkey_row = vbox({
        text(i18n::tr(i18n::I18nKey::PUBLIC_KEY_LABEL)) | color(palette::fg_dark()),
        text(pk_display) | color(palette::cyan()),
    });
    
    auto import_export = hbox({
        export_button_persistent_->Render(),
        text("  "),
        import_button_persistent_->Render(),
    });

    auto danger_zone = vbox({
        text(i18n::tr(i18n::I18nKey::DANGER_ZONE)) | bold | color(palette::red()),
        separator(),
        logout_button_persistent_->Render() | color(palette::red()),
    });
    
    return vbox({
        text(i18n::tr(i18n::I18nKey::ACCOUNT_SETTINGS)) | bold | color(palette::blue()),
        separator(),
        nickname_row,
        text(i18n::tr(i18n::I18nKey::NICKNAME_HINT)) | color(palette::comment()) | dim,
        text(""),
        pubkey_row,
        text(""),
        text(i18n::tr(i18n::I18nKey::IMPORT_EXPORT)) | bold | color(palette::blue()),
        separator(),
        import_export,
        text(""),
        danger_zone,
    });
}

void SettingsScreen::load_settings_from_config(const ClientConfig& cfg) {
    // Voice device options (label list + value list)
    voice_input_device_values_.clear();
    voice_output_device_values_.clear();
    voice_input_device_options_.clear();
    voice_output_device_options_.clear();
    voice_mode_options_ = {
        i18n::tr(i18n::I18nKey::VOICE_MODE_PTT),
        i18n::tr(i18n::I18nKey::VOICE_MODE_VOX),
    };
    voice_input_device_values_.push_back("");
    voice_output_device_values_.push_back("");
    voice_input_device_options_.push_back(i18n::tr(i18n::I18nKey::VOICE_SYSTEM_DEFAULT));
    voice_output_device_options_.push_back(i18n::tr(i18n::I18nKey::VOICE_SYSTEM_DEFAULT));
    for (const auto& name : voice::AudioDevice::list_input_devices()) {
        voice_input_device_values_.push_back(name);
        voice_input_device_options_.push_back(name);
    }
    for (const auto& name : voice::AudioDevice::list_output_devices()) {
        voice_output_device_values_.push_back(name);
        voice_output_device_options_.push_back(name);
    }

    // Appearance
    theme_ = cfg.ui.theme;
    theme_selected_ = theme_to_index(theme_options_.empty() ? available_themes() : theme_options_, theme_);
    timestamp_format_ = cfg.ui.timestamp_format;
    max_messages_ = std::to_string(cfg.ui.max_messages);
    font_scale_ = cfg.ui.font_scale;
    show_timestamps_ = cfg.ui.show_timestamps;
    show_user_colors_ = cfg.ui.show_user_colors;
    language_ = cfg.ui.language;
    language_selected_ = language_to_index(language_);
    copy_selection_on_release_ = cfg.ui.copy_selection_on_release;

    // General preview settings
    inline_images_ = cfg.preview.inline_images;
    image_columns_ = std::to_string(cfg.preview.image_columns);
    image_rows_ = std::to_string(cfg.preview.image_rows);
    terminal_graphics_ = cfg.preview.terminal_graphics;
    terminal_graphics_selected_ = terminal_graphics_to_index(terminal_graphics_);

    // Voice
    voice_input_device_selected_ = find_device_index(voice_input_device_values_, cfg.voice.input_device);
    voice_output_device_selected_ = find_device_index(voice_output_device_values_, cfg.voice.output_device);
    voice_mode_selected_ = voice_mode_to_index(cfg.voice.mode);
    voice_noise_suppression_enabled_ = cfg.voice.noise_suppression_enabled;
    voice_noise_suppression_level_selected_ = noise_suppression_level_to_index(cfg.voice.noise_suppression_level);
    voice_limiter_enabled_ = cfg.voice.limiter_enabled;
    voice_limiter_threshold_percent_ = std::clamp(static_cast<int>(cfg.voice.limiter_threshold * 100.0f + 0.5f), 20, 99);
    voice_ptt_key_ = cfg.voice.ptt_key;
    voice_vad_threshold_percent_ = std::clamp(
        static_cast<int>(cfg.voice.vad_threshold * 100.0f + 0.5f), 0, 100);
    voice_jitter_buffer_frames_ = std::clamp(cfg.voice.jitter_buffer_frames, 2, 10);
    voice_input_volume_value_ = std::clamp(cfg.voice.input_volume, 0, 200);
    voice_output_volume_value_ = std::clamp(cfg.voice.output_volume, 0, 200);
    
    // Connection
    auto_reconnect_ = cfg.connection.auto_reconnect;
    reconnect_delay_sec_ = cfg.connection.reconnect_delay_sec;
    reconnect_delay_sec_text_ = std::to_string(reconnect_delay_sec_);
    connection_timeout_sec_ = cfg.connection.timeout_sec;
    connection_timeout_sec_text_ = std::to_string(connection_timeout_sec_);
    tls_verify_peer_ = cfg.tls.verify_peer;
    tls_use_custom_cert_ = !cfg.server.cert_pin.empty();
    tls_cert_pin_ = cfg.server.cert_pin;
    
    // Notifications
    desktop_notifications_ = cfg.notifications.desktop_notifications;
    sound_alerts_ = cfg.notifications.sound_alerts;
    notify_on_mention_ = cfg.notifications.notify_on_mention;
    notify_on_dm_ = cfg.notifications.notify_on_dm;
    mention_keywords_ = cfg.notifications.mention_keywords;

    // Privacy
    share_typing_indicators_ = cfg.privacy.share_typing_indicators;
    share_read_receipts_ = cfg.privacy.share_read_receipts;
    
    // Account
    nickname_ = cfg.identity.user_id;
}

void SettingsScreen::save_settings_to_config(ClientConfig& cfg) {
    auto parse_int_or = [](const std::string& value, int fallback) {
        try {
            return std::stoi(value);
        } catch (...) {
            return fallback;
        }
    };
    auto clamp_int = [](int value, int lo, int hi) {
        return std::min(hi, std::max(lo, value));
    };
    // Appearance
    theme_ = theme_from_index(theme_options_, theme_selected_);
    cfg.ui.theme = theme_;
    cfg.ui.timestamp_format = timestamp_format_;
    cfg.ui.max_messages = parse_int_or(max_messages_, kDefaultMaxMessages);
    cfg.ui.font_scale = font_scale_;
    cfg.ui.show_timestamps = show_timestamps_;
    cfg.ui.show_user_colors = show_user_colors_;
    cfg.ui.copy_selection_on_release = copy_selection_on_release_;
    language_ = language_from_index(language_selected_);
    cfg.ui.language = language_;

    // General preview settings
    cfg.preview.inline_images = inline_images_;
    cfg.preview.image_columns = clamp_int(parse_int_or(image_columns_, cfg.preview.image_columns), 1, 400);
    cfg.preview.image_rows = clamp_int(parse_int_or(image_rows_, cfg.preview.image_rows), 1, 200);
    terminal_graphics_ = terminal_graphics_from_index(terminal_graphics_selected_);
    cfg.preview.terminal_graphics = terminal_graphics_;
    image_columns_ = std::to_string(cfg.preview.image_columns);
    image_rows_ = std::to_string(cfg.preview.image_rows);
    terminal_graphics_selected_ = terminal_graphics_to_index(cfg.preview.terminal_graphics);

    // Voice settings
    if (voice_input_device_selected_ >= 0 &&
        voice_input_device_selected_ < static_cast<int>(voice_input_device_values_.size())) {
        cfg.voice.input_device = voice_input_device_values_[static_cast<size_t>(voice_input_device_selected_)];
    }
    if (voice_output_device_selected_ >= 0 &&
        voice_output_device_selected_ < static_cast<int>(voice_output_device_values_.size())) {
        cfg.voice.output_device = voice_output_device_values_[static_cast<size_t>(voice_output_device_selected_)];
    }
    cfg.voice.mode = voice_mode_from_index(voice_mode_selected_);
    cfg.voice.noise_suppression_enabled = voice_noise_suppression_enabled_;
    cfg.voice.noise_suppression_level = noise_suppression_level_from_index(voice_noise_suppression_level_selected_);
    cfg.voice.limiter_enabled = voice_limiter_enabled_;
    cfg.voice.limiter_threshold =
        std::clamp(static_cast<float>(voice_limiter_threshold_percent_) / 100.0f, 0.20f, 0.99f);
    cfg.voice.ptt_key = voice_ptt_key_.empty() ? "F1" : voice_ptt_key_;
    cfg.voice.vad_threshold =
        std::clamp(static_cast<float>(voice_vad_threshold_percent_) / 100.0f, 0.0f, 1.0f);
    cfg.voice.jitter_buffer_frames = clamp_int(voice_jitter_buffer_frames_, 2, 10);
    cfg.voice.input_volume = clamp_int(voice_input_volume_value_, 0, 200);
    cfg.voice.output_volume = clamp_int(voice_output_volume_value_, 0, 200);

    // Connection
    cfg.connection.auto_reconnect = auto_reconnect_;
    cfg.connection.reconnect_delay_sec = clamp_int(parse_int_or(reconnect_delay_sec_text_, kDefaultReconnectDelay), 1, 300);
    cfg.connection.timeout_sec = clamp_int(parse_int_or(connection_timeout_sec_text_, kDefaultTimeout), 1, 300);
    reconnect_delay_sec_text_ = std::to_string(cfg.connection.reconnect_delay_sec);
    connection_timeout_sec_text_ = std::to_string(cfg.connection.timeout_sec);
    
    cfg.tls.verify_peer = tls_verify_peer_;
    cfg.server.cert_pin = tls_cert_pin_;

    // Notifications
    cfg.notifications.desktop_notifications = desktop_notifications_;
    cfg.notifications.sound_alerts = sound_alerts_;
    cfg.notifications.notify_on_mention = notify_on_mention_;
    cfg.notifications.notify_on_dm = notify_on_dm_;
    cfg.notifications.mention_keywords = mention_keywords_;

    // Privacy
    cfg.privacy.share_typing_indicators = share_typing_indicators_;
    cfg.privacy.share_read_receipts = share_read_receipts_;
    
    // Account
    if (!nickname_.empty() && nickname_ != cfg.identity.user_id) {
        cfg.identity.user_id = nickname_;
    }
    
    // Notify theme change
    if (on_theme_change_ && theme_ != original_cfg_.ui.theme) {
        on_theme_change_(theme_);
    }
}

void SettingsScreen::reset_to_defaults() {
    // Appearance defaults
    theme_ = "tokyo-night";
    theme_selected_ = theme_to_index(theme_options_.empty() ? available_themes() : theme_options_, theme_);
    font_scale_ = kDefaultFontScale;
    show_timestamps_ = kDefaultShowTimestamps;
    show_user_colors_ = kDefaultShowUserColors;
    timestamp_format_ = "%H:%M";
    max_messages_ = std::to_string(kDefaultMaxMessages);
    copy_selection_on_release_ = true;
    inline_images_ = true;
    image_columns_ = "40";
    image_rows_ = "16";
    terminal_graphics_ = "auto";
    terminal_graphics_selected_ = terminal_graphics_to_index(terminal_graphics_);
    language_ = "fi";
    language_selected_ = language_to_index(language_);

    // Voice defaults
    voice_input_device_selected_ = 0;
    voice_output_device_selected_ = 0;
    voice_mode_selected_ = 0;
    voice_noise_suppression_enabled_ = true;
    voice_noise_suppression_level_selected_ = 1;
    voice_limiter_enabled_ = true;
    voice_limiter_threshold_percent_ = 85;
    voice_ptt_key_ = "F1";
    voice_vad_threshold_percent_ = 2;
    voice_jitter_buffer_frames_ = 4;
    voice_input_volume_value_ = 100;
    voice_output_volume_value_ = 100;
    
    // Connection defaults
    auto_reconnect_ = kDefaultAutoReconnect;
    reconnect_delay_sec_ = kDefaultReconnectDelay;
    reconnect_delay_sec_text_ = std::to_string(kDefaultReconnectDelay);
    connection_timeout_sec_ = kDefaultTimeout;
    connection_timeout_sec_text_ = std::to_string(kDefaultTimeout);
    tls_verify_peer_ = kDefaultTlsVerify;
    tls_use_custom_cert_ = false;
    tls_cert_pin_.clear();
    
    // Notification defaults
    desktop_notifications_ = kDefaultDesktopNotifications;
    sound_alerts_ = kDefaultSoundAlerts;
    notify_on_mention_ = kDefaultNotifyMention;
    notify_on_dm_ = kDefaultNotifyDM;
    mention_keywords_ = nickname_;
    share_typing_indicators_ = kDefaultShareTypingIndicators;
    share_read_receipts_ = kDefaultShareReadReceipts;
}

void SettingsScreen::export_settings() {
    try {
        auto export_path = original_cfg_.config_dir / "settings_backup.toml";
        std::filesystem::create_directories(export_path.parent_path());
        
        toml::value data;
        data["ui"]["theme"] = theme_;
        data["ui"]["timestamp_format"] = timestamp_format_;
        data["ui"]["max_messages"] = std::stoi(max_messages_);
        data["ui"]["copy_selection_on_release"] = copy_selection_on_release_;
        data["preview"]["inline_images"] = inline_images_;
        data["preview"]["image_columns"] = std::stoi(image_columns_);
        data["preview"]["image_rows"] = std::stoi(image_rows_);
        data["preview"]["terminal_graphics"] = terminal_graphics_;
        data["voice"]["input_device"] = (voice_input_device_selected_ >= 0 &&
            voice_input_device_selected_ < static_cast<int>(voice_input_device_values_.size()))
            ? voice_input_device_values_[static_cast<size_t>(voice_input_device_selected_)] : "";
        data["voice"]["output_device"] = (voice_output_device_selected_ >= 0 &&
            voice_output_device_selected_ < static_cast<int>(voice_output_device_values_.size()))
            ? voice_output_device_values_[static_cast<size_t>(voice_output_device_selected_)] : "";
        data["voice"]["input_volume"] = voice_input_volume_value_;
        data["voice"]["output_volume"] = voice_output_volume_value_;
        data["voice"]["mode"] = voice_mode_from_index(voice_mode_selected_);
        data["voice"]["noise_suppression_enabled"] = voice_noise_suppression_enabled_;
        data["voice"]["noise_suppression_level"] = noise_suppression_level_from_index(voice_noise_suppression_level_selected_);
        data["voice"]["jitter_buffer_frames"] = voice_jitter_buffer_frames_;
        data["voice"]["limiter_enabled"] = voice_limiter_enabled_;
        data["voice"]["limiter_threshold"] = static_cast<double>(voice_limiter_threshold_percent_) / 100.0;
        data["voice"]["ptt_key"] = voice_ptt_key_;
        data["voice"]["vad_threshold"] = static_cast<double>(voice_vad_threshold_percent_) / 100.0;
        data["voice"]["jitter_buffer_frames"] = voice_jitter_buffer_frames_;
        
        data["connection"]["auto_reconnect"] = auto_reconnect_;
        data["connection"]["reconnect_delay"] = std::stoi(reconnect_delay_sec_text_);
        data["connection"]["timeout"] = std::stoi(connection_timeout_sec_text_);
        
        data["notifications"]["desktop"] = desktop_notifications_;
        data["notifications"]["sound"] = sound_alerts_;
        data["notifications"]["on_mention"] = notify_on_mention_;
        data["notifications"]["keywords"] = mention_keywords_;
        data["privacy"]["share_typing_indicators"] = share_typing_indicators_;
        data["privacy"]["share_read_receipts"] = share_read_receipts_;
        
        std::ofstream ofs(export_path);
        ofs << data;
        
        spdlog::info("Settings exported to: {}", export_path.string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to export settings: {}", e.what());
    }
}

void SettingsScreen::import_settings() {
    try {
        auto import_path = original_cfg_.config_dir / "settings_backup.toml";
        if (!std::filesystem::exists(import_path)) {
            spdlog::warn("No backup file found at: {}", import_path.string());
            return;
        }
        
        auto data = toml::parse(import_path.string());
        
        if (data.contains("ui")) {
            auto& ui = data.at("ui");
            if (ui.contains("theme")) {
                theme_ = toml::find<std::string>(ui, "theme");
                theme_selected_ = theme_to_index(theme_options_.empty() ? available_themes() : theme_options_, theme_);
            }
            if (ui.contains("timestamp_format")) timestamp_format_ = toml::find<std::string>(ui, "timestamp_format");
            if (ui.contains("max_messages")) max_messages_ = std::to_string(toml::find<int>(ui, "max_messages"));
            if (ui.contains("language")) {
                language_ = toml::find<std::string>(ui, "language");
                language_selected_ = language_to_index(language_);
            }
            if (ui.contains("copy_selection_on_release")) {
                copy_selection_on_release_ = toml::find<bool>(ui, "copy_selection_on_release");
            }
        }

        if (data.contains("preview")) {
            auto& preview = data.at("preview");
            if (preview.contains("inline_images")) inline_images_ = toml::find<bool>(preview, "inline_images");
            if (preview.contains("image_columns")) image_columns_ = std::to_string(toml::find<int>(preview, "image_columns"));
            if (preview.contains("image_rows")) image_rows_ = std::to_string(toml::find<int>(preview, "image_rows"));
            if (preview.contains("terminal_graphics")) {
                terminal_graphics_ = toml::find<std::string>(preview, "terminal_graphics");
                terminal_graphics_selected_ = terminal_graphics_to_index(terminal_graphics_);
            }
        }

        if (data.contains("voice")) {
            auto& voice = data.at("voice");
            if (voice.contains("input_device")) {
                voice_input_device_selected_ = find_device_index(
                    voice_input_device_values_, toml::find<std::string>(voice, "input_device"));
            }
            if (voice.contains("output_device")) {
                voice_output_device_selected_ = find_device_index(
                    voice_output_device_values_, toml::find<std::string>(voice, "output_device"));
            }
            if (voice.contains("input_volume")) {
                voice_input_volume_value_ = std::clamp(toml::find<int>(voice, "input_volume"), 0, 200);
            }
            if (voice.contains("output_volume")) {
                voice_output_volume_value_ = std::clamp(toml::find<int>(voice, "output_volume"), 0, 200);
            }
            if (voice.contains("mode")) {
                voice_mode_selected_ = voice_mode_to_index(toml::find<std::string>(voice, "mode"));
            }
            if (voice.contains("noise_suppression_enabled")) {
                voice_noise_suppression_enabled_ = toml::find<bool>(voice, "noise_suppression_enabled");
            }
            if (voice.contains("noise_suppression_level")) {
                voice_noise_suppression_level_selected_ =
                    noise_suppression_level_to_index(toml::find<std::string>(voice, "noise_suppression_level"));
            }
            if (voice.contains("limiter_enabled")) {
                voice_limiter_enabled_ = toml::find<bool>(voice, "limiter_enabled");
            }
            if (voice.contains("limiter_threshold")) {
                voice_limiter_threshold_percent_ = std::clamp(
                    static_cast<int>(toml::find<double>(voice, "limiter_threshold") * 100.0 + 0.5), 20, 99);
            }
            if (voice.contains("ptt_key")) {
                voice_ptt_key_ = toml::find<std::string>(voice, "ptt_key");
            }
            if (voice.contains("vad_threshold")) {
                voice_vad_threshold_percent_ = std::clamp(
                    static_cast<int>(toml::find<double>(voice, "vad_threshold") * 100.0 + 0.5), 0, 100);
            }
            if (voice.contains("jitter_buffer_frames")) {
                voice_jitter_buffer_frames_ = std::clamp(toml::find<int>(voice, "jitter_buffer_frames"), 2, 10);
            }
        }
        
        if (data.contains("connection")) {
            
            auto& conn = data.at("connection");
            if (conn.contains("auto_reconnect")) auto_reconnect_ = toml::find<bool>(conn, "auto_reconnect");
            if (conn.contains("reconnect_delay")) reconnect_delay_sec_text_ = std::to_string(toml::find<int>(conn, "reconnect_delay"));
            if (conn.contains("timeout")) connection_timeout_sec_text_ = std::to_string(toml::find<int>(conn, "timeout"));
        }
        
        if (data.contains("notifications")) {
            auto& notif = data.at("notifications");
            if (notif.contains("desktop")) desktop_notifications_ = toml::find<bool>(notif, "desktop");
            if (notif.contains("sound")) sound_alerts_ = toml::find<bool>(notif, "sound");
            if (notif.contains("on_mention")) notify_on_mention_ = toml::find<bool>(notif, "on_mention");
            if (notif.contains("keywords")) mention_keywords_ = toml::find<std::string>(notif, "keywords");
        }

        if (data.contains("privacy")) {
            auto& privacy = data.at("privacy");
            if (privacy.contains("share_typing_indicators")) {
                share_typing_indicators_ = toml::find<bool>(privacy, "share_typing_indicators");
            }
            if (privacy.contains("share_read_receipts")) {
                share_read_receipts_ = toml::find<bool>(privacy, "share_read_receipts");
            }
        }
        
        spdlog::info("Settings imported from: {}", import_path.string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to import settings: {}", e.what());
    }
}

} // namespace grotto::ui
