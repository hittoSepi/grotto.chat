#include "ui/settings_screen.hpp"
#include "ui/color_scheme.hpp"
#include "ui/key_name.hpp"
#include "ui/modal_overlay.hpp"
#include "i18n/strings.hpp"
#include "voice/audio_device.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/util/autoreset.hpp>
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
#include <memory>
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
constexpr bool kDefaultAutoAwayEnabled = false;
constexpr int kDefaultAutoAwayMinutes = 10;
constexpr bool kDefaultShowTimestamps = true;
constexpr bool kDefaultShowUserColors = true;
constexpr int kDefaultFontScale = 100;
constexpr int kDefaultMaxMessages = 1000;
constexpr int kMinSettingsWidth = 120;

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

int find_device_index(const std::vector<std::string>& values,
                      const std::string& selected_value) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == selected_value) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

Element yframe_scrolled(Element child, int* offset, int* max_offset, bool* follow_focus) {
    class ScrolledFrame final : public Node {
    public:
        ScrolledFrame(Elements children, int* offset, int* max_offset, bool* follow_focus)
            : Node(std::move(children)),
              offset_(offset),
              max_offset_(max_offset),
              follow_focus_(follow_focus) {}

        void ComputeRequirement() override {
            Node::ComputeRequirement();
            requirement_ = children_[0]->requirement();
        }

        void SetBox(Box box) override {
            Node::SetBox(box);

            auto children_box = box;
            const auto& focused_box = requirement_.focused.box;
            const int external_dimy = box.y_max - box.y_min;
            const int internal_dimy = std::max(requirement_.min_y, external_dimy);
            const int max_scroll = std::max(0, internal_dimy - external_dimy - 1);

            *max_offset_ = max_scroll;
            *offset_ = std::clamp(*offset_, 0, max_scroll);

            if (*follow_focus_) {
                const int focused_dimy = focused_box.y_max - focused_box.y_min;
                int target = focused_box.y_min - external_dimy / 2 + focused_dimy / 2;
                *offset_ = std::clamp(target, 0, max_scroll);
            }

            children_box.y_min = box.y_min - *offset_;
            children_box.y_max = box.y_min + internal_dimy - *offset_;
            children_[0]->SetBox(children_box);
        }

        void Render(Screen& screen) override {
            const AutoReset<Box> stencil(&screen.stencil, Box::Intersection(box_, screen.stencil));
            children_[0]->Render(screen);
        }

    private:
        int* offset_;
        int* max_offset_;
        bool* follow_focus_;
    };

    return std::make_shared<ScrolledFrame>(unpack(std::move(child)), offset, max_offset, follow_focus);
}

bool box_contains(const Box& box, int x, int y) {
    return x >= box.x_min && x <= box.x_max && y >= box.y_min && y <= box.y_max;
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
    content_scroll_offset_ = 0;
    content_scroll_follow_focus_ = true;
    if (content_container_) {
        content_container_->TakeFocus();
    }
}

SettingsResult SettingsScreen::show(ClientConfig& cfg,
                                    ftxui::ScreenInteractive& screen,
                                    const std::string& public_key_hex,
                                    ThemeChangeFn on_theme_change,
                                    VoiceTestToggleFn on_voice_test_toggle,
                                    VoiceTestStateFn voice_test_state,
                                    VoiceTestMetricsFn voice_test_metrics) {
    saved_ = false;
    cancelled_ = false;
    logout_ = false;
    original_cfg_ = cfg;
    public_key_hex_ = public_key_hex;
    on_theme_change_ = on_theme_change;
    on_voice_test_toggle_ = on_voice_test_toggle;
    voice_test_state_ = voice_test_state;
    voice_test_metrics_ = voice_test_metrics;
    config_path_ = cfg.config_dir / "config.toml";

    auto exit_closure = screen.ExitLoopClosure();
    exit_closure_ = exit_closure;
    
    load_settings_from_config(cfg);
    voice_test_active_ = voice_test_state_ ? voice_test_state_() : false;
    content_scroll_offset_ = 0;
    content_scroll_max_offset_ = 0;
    content_scroll_follow_focus_ = true;
    build_ui();
    if (content_container_) {
        content_container_->TakeFocus();
    }
    
#ifndef _WIN32
    install_interrupt_handlers();
#endif
    screen.ForceHandleCtrlC(false);
    
    // Main renderer
    auto renderer = Renderer(container_, [this, &screen] {
        if (screen.dimx() < kMinSettingsWidth) {
            return vbox({
                filler(),
                vbox({
                    text(i18n::tr(i18n::I18nKey::SETTINGS_MIN_WIDTH_TITLE)) | bold | color(palette::red()) | center,
                    separator(),
                    paragraphAlignLeft(i18n::tr(i18n::I18nKey::SETTINGS_MIN_WIDTH_BODY,
                                                std::to_string(kMinSettingsWidth),
                                                std::to_string(screen.dimx()))) | center,
                    text("") | center,
                    paragraphAlignLeft(i18n::tr(i18n::I18nKey::SETTINGS_MIN_WIDTH_HINT)) | color(palette::comment()) | dim | center,
                }) | border | size(WIDTH, LESS_THAN, 72) | center,
                filler(),
            }) | bgcolor(palette::bg()) | color(palette::fg());
        }

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
            yframe_scrolled(std::move(content),
                            &content_scroll_offset_,
                            &content_scroll_max_offset_,
                            &content_scroll_follow_focus_)
                | vscroll_indicator
                | reflect(content_scroll_box_)
                | flex,
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
        if (event == Event::PageDown) {
            const int page_step = std::max(3, content_scroll_box_.y_max - content_scroll_box_.y_min - 1);
            content_scroll_follow_focus_ = false;
            content_scroll_offset_ =
                std::min(content_scroll_offset_ + page_step, content_scroll_max_offset_);
            return true;
        }
        if (event == Event::PageUp) {
            const int page_step = std::max(3, content_scroll_box_.y_max - content_scroll_box_.y_min - 1);
            content_scroll_follow_focus_ = false;
            content_scroll_offset_ = std::max(content_scroll_offset_ - page_step, 0);
            return true;
        }
        if (event.is_mouse()) {
            const auto& mouse = event.mouse();
            if (mouse.button == Mouse::WheelDown &&
                box_contains(content_scroll_box_, mouse.x, mouse.y)) {
                content_scroll_follow_focus_ = false;
                content_scroll_offset_ =
                    std::min(content_scroll_offset_ + 3, content_scroll_max_offset_);
                return true;
            }
            if (mouse.button == Mouse::WheelUp &&
                box_contains(content_scroll_box_, mouse.x, mouse.y)) {
                content_scroll_follow_focus_ = false;
                content_scroll_offset_ = std::max(content_scroll_offset_ - 3, 0);
                return true;
            }
            if (mouse.button == Mouse::Left &&
                box_contains(content_scroll_box_, mouse.x, mouse.y)) {
                content_scroll_follow_focus_ = true;
            }
        }
        if (event == Event::Tab ||
            event == Event::TabReverse ||
            event == Event::ArrowUp ||
            event == Event::ArrowDown ||
            event == Event::ArrowLeft ||
            event == Event::ArrowRight ||
            event == Event::Home ||
            event == Event::End ||
            event == Event::Return) {
            content_scroll_follow_focus_ = true;
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
    auto make_sidebar_button = [this](const std::string& label, SettingsCategory category) {
        auto button = Button(" " + label + " ", [this, category] { set_active_category(category); });
        return Renderer(button, [this, button, category] {
            auto element = button->Render();
            if (active_category_ == category) {
                return element | bold | bgcolor(palette::fg()) | color(palette::bg_dark());
            }
            return element;
        });
    };

    // Sidebar category buttons
    sidebar_container_ = Container::Vertical({
        make_sidebar_button(i18n::tr(i18n::I18nKey::CATEGORY_GENERAL), SettingsCategory::General),
        make_sidebar_button(i18n::tr(i18n::I18nKey::CATEGORY_APPEARANCE), SettingsCategory::Appearance),
        make_sidebar_button(i18n::tr(i18n::I18nKey::CATEGORY_VOICE), SettingsCategory::Voice),
        make_sidebar_button(i18n::tr(i18n::I18nKey::CATEGORY_CONNECTION), SettingsCategory::Connection),
        make_sidebar_button(i18n::tr(i18n::I18nKey::CATEGORY_NOTIFICATIONS), SettingsCategory::Notifications),
        make_sidebar_button(i18n::tr(i18n::I18nKey::CATEGORY_PRIVACY), SettingsCategory::Privacy),
        make_sidebar_button(i18n::tr(i18n::I18nKey::CATEGORY_ACCOUNT), SettingsCategory::Account),
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
    language_options_ = {
        i18n::tr(i18n::I18nKey::LANGUAGE_NAME_FINNISH),
        i18n::tr(i18n::I18nKey::LANGUAGE_NAME_ENGLISH),
    };
    voice_input_device_dropdown_ = Dropdown(&voice_input_device_options_, &voice_input_device_selected_);
    voice_output_device_dropdown_ = Dropdown(&voice_output_device_options_, &voice_output_device_selected_);
    voice_mode_dropdown_ = Dropdown(&voice_mode_options_, &voice_mode_selected_);
    voice_noise_suppression_cb_ = Checkbox(i18n::tr(i18n::I18nKey::VOICE_NOISE_SUPPRESSION_LABEL), &voice_noise_suppression_enabled_);
    voice_capture_key_button_ = Button(i18n::tr(i18n::I18nKey::VOICE_SET_HOTKEY_BUTTON), [this] {
        voice_key_capture_visible_ = true;
    });
    voice_vad_threshold_slider_ = Slider("", &voice_vad_threshold_percent_, 0, 100, 1);
    voice_jitter_buffer_slider_ = Slider("", &voice_jitter_buffer_frames_, 2, 10, 1);
    voice_limiter_cb_ = Checkbox(i18n::tr(i18n::I18nKey::VOICE_LIMITER_LABEL), &voice_limiter_enabled_);
    voice_limiter_threshold_slider_ = Slider("", &voice_limiter_threshold_percent_, 20, 99, 1);
    voice_input_volume_slider_ = Slider("", &voice_input_volume_value_, 0, 200, 1);
    voice_output_volume_slider_ = Slider("", &voice_output_volume_value_, 0, 200, 1);
    voice_self_test_button_ = Button(i18n::tr(i18n::I18nKey::VOICE_SELF_TEST_BUTTON), [this] {
        if (!on_voice_test_toggle_) {
            voice_test_active_ = false;
            return;
        }
        ClientConfig preview_cfg = original_cfg_;
        apply_settings_to_config(preview_cfg, false);
        voice_test_active_ = on_voice_test_toggle_(preview_cfg);
    });
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
    auto_away_cb_ = Checkbox(i18n::tr(i18n::I18nKey::AUTO_AWAY_ENABLED), &auto_away_enabled_);
    auto_away_minutes_input_ = Input(&auto_away_minutes_, "10");

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
        show_timestamps_cb_,
        show_user_colors_cb_,
        timestamp_format_input_,
        max_messages_input_,
        language_toggle_,
    });

    voice_container_ = Container::Vertical({
        voice_input_device_dropdown_,
        voice_output_device_dropdown_,
        voice_input_volume_slider_,
        voice_output_volume_slider_,
        voice_mode_dropdown_,
        voice_capture_key_button_,
        voice_vad_threshold_slider_,
        voice_noise_suppression_cb_,
        voice_limiter_cb_,
        voice_limiter_threshold_slider_,
        voice_jitter_buffer_slider_,
        voice_self_test_button_,
    });

    connection_container_ = Container::Vertical({
        auto_reconnect_cb_,
        reconnect_delay_input_,
        timeout_input_,
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
        auto_away_cb_,
        auto_away_minutes_input_,
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
    auto_away_enabled_ = cfg.privacy.auto_away_enabled;
    auto_away_minutes_ = std::to_string(std::clamp(cfg.privacy.auto_away_minutes, 1, 240));
    
    // Account
    nickname_ = cfg.identity.user_id;
}

void SettingsScreen::apply_settings_to_config(ClientConfig& cfg, bool notify_theme_change) {
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
    cfg.voice.limiter_enabled = voice_limiter_enabled_;
    cfg.voice.limiter_threshold =
        std::clamp(static_cast<float>(voice_limiter_threshold_percent_) / 100.0f, 0.20f, 0.99f);
    cfg.voice.ptt_key = voice_ptt_key_.empty() ? "§" : voice_ptt_key_;
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
    cfg.privacy.auto_away_enabled = auto_away_enabled_;
    cfg.privacy.auto_away_minutes = clamp_int(parse_int_or(auto_away_minutes_, kDefaultAutoAwayMinutes), 1, 240);
    auto_away_minutes_ = std::to_string(cfg.privacy.auto_away_minutes);
    
    // Account
    if (!nickname_.empty() && nickname_ != cfg.identity.user_id) {
        cfg.identity.user_id = nickname_;
    }
    
    // Notify theme change
    if (notify_theme_change && on_theme_change_ && theme_ != original_cfg_.ui.theme) {
        on_theme_change_(theme_);
    }
}

void SettingsScreen::save_settings_to_config(ClientConfig& cfg) {
    apply_settings_to_config(cfg, true);
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
    voice_limiter_enabled_ = true;
    voice_limiter_threshold_percent_ = 85;
    voice_ptt_key_ = "§";
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
    auto_away_enabled_ = kDefaultAutoAwayEnabled;
    auto_away_minutes_ = std::to_string(kDefaultAutoAwayMinutes);
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
        data["privacy"]["auto_away_enabled"] = auto_away_enabled_;
        data["privacy"]["auto_away_minutes"] = std::clamp(std::stoi(auto_away_minutes_), 1, 240);
        
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
            if (privacy.contains("auto_away_enabled")) {
                auto_away_enabled_ = toml::find<bool>(privacy, "auto_away_enabled");
            }
            if (privacy.contains("auto_away_minutes")) {
                auto_away_minutes_ = std::to_string(std::clamp(toml::find<int>(privacy, "auto_away_minutes"), 1, 240));
            }
        }
        
        spdlog::info("Settings imported from: {}", import_path.string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to import settings: {}", e.what());
    }
}

} // namespace grotto::ui
