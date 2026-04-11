#pragma once

#include "config.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <functional>
#include <string>
#include <vector>

namespace grotto::ui {

// Settings categories
enum class SettingsCategory {
    General,
    Appearance,
    Voice,
    Connection,
    Notifications,
    Privacy,
    Account
};

// Result of the settings session
enum class SettingsResult {
    Saved,      // User saved changes
    Cancelled,  // User cancelled without saving
    Logout,     // User chose to logout
};

// Callback for theme changes (applied immediately)
using ThemeChangeFn = std::function<void(const std::string& theme_name)>;
using VoiceTestToggleFn = std::function<bool(const ClientConfig& preview_cfg)>;
using VoiceTestStateFn = std::function<bool()>;

struct VoiceTestMetrics {
    float input_rms = 0.0f;
    float input_peak = 0.0f;
    bool vad_open = false;
    bool limiter_active = false;
    bool clipped = false;
    int loopback_buffer_ms = 0;
};

using VoiceTestMetricsFn = std::function<VoiceTestMetrics()>;

// SettingsScreen handles the settings UI with category sidebar.
// It blocks until the user saves, cancels, or logs out.
class SettingsScreen {
public:
    SettingsScreen();

    // Run the settings screen modally. Returns the result.
    // Updates the provided config with any changes made.
    SettingsResult show(ClientConfig& cfg,
                        ftxui::ScreenInteractive& screen,
                        const std::string& public_key_hex,
                        ThemeChangeFn on_theme_change = nullptr,
                        VoiceTestToggleFn on_voice_test_toggle = nullptr,
                        VoiceTestStateFn voice_test_state = nullptr,
                        VoiceTestMetricsFn voice_test_metrics = nullptr);

private:
    void build_ui();
    void set_active_category(SettingsCategory category);
    void apply_settings_to_config(ClientConfig& cfg, bool notify_theme_change);
    void save_settings_to_config(ClientConfig& cfg);
    void load_settings_from_config(const ClientConfig& cfg);
    void reset_to_defaults();
    void export_settings();
    void import_settings();
    
    // Category renderers
    ftxui::Element render_general();
    ftxui::Element render_appearance();
    ftxui::Element render_voice();
    ftxui::Element render_connection();
    ftxui::Element render_notifications();
    ftxui::Element render_privacy();
    ftxui::Element render_account();

    // UI State
    SettingsCategory active_category_ = SettingsCategory::General;
    int active_category_index_ = 0;
    bool saved_ = false;
    bool cancelled_ = false;
    bool logout_ = false;
    
    // Original config for cancel support
    ClientConfig original_cfg_;
    
    // Theme change callback
    ThemeChangeFn on_theme_change_;
    VoiceTestToggleFn on_voice_test_toggle_;
    VoiceTestStateFn voice_test_state_;
    VoiceTestMetricsFn voice_test_metrics_;
    std::function<void()> exit_closure_;
    
    // Config paths for import/export
    std::filesystem::path config_path_;

    // === Appearance Settings ===
    std::string theme_;
    int theme_selected_ = 0;
    std::vector<std::string> theme_options_;
    int font_scale_;
    bool show_timestamps_;
    bool show_user_colors_;
    std::string timestamp_format_;
    std::string max_messages_;
    std::string language_;
    bool copy_selection_on_release_;
    bool inline_images_;
    std::string image_columns_;
    std::string image_rows_;
    std::string terminal_graphics_;
    int terminal_graphics_selected_ = 0;
    std::vector<std::string> terminal_graphics_options_ = {"auto", "off", "viewer-only"};
    int language_selected_ = 0;
    std::vector<std::string> language_options_ = {"Suomi", "English"};

    // === Voice Settings ===
    std::vector<std::string> voice_input_device_values_;
    std::vector<std::string> voice_output_device_values_;
    std::vector<std::string> voice_input_device_options_;
    std::vector<std::string> voice_output_device_options_;
    int voice_input_device_selected_ = 0;
    int voice_output_device_selected_ = 0;
    std::vector<std::string> voice_mode_options_ = {"PTT", "Voice Activation"};
    int voice_mode_selected_ = 0;
    bool voice_noise_suppression_enabled_ = true;
    std::vector<std::string> voice_noise_suppression_level_values_ = {"low", "moderate", "high", "very_high"};
    std::vector<std::string> voice_noise_suppression_level_options_;
    int voice_noise_suppression_level_selected_ = 1;
    std::string voice_ptt_key_;
    int voice_vad_threshold_percent_ = 2;
    int voice_jitter_buffer_frames_ = 4;
    bool voice_limiter_enabled_ = true;
    int voice_limiter_threshold_percent_ = 85;
    int voice_input_volume_value_ = 100;
    int voice_output_volume_value_ = 100;
    bool voice_key_capture_visible_ = false;
    bool voice_test_active_ = false;

    // === Connection Settings ===
    bool auto_reconnect_;
    int reconnect_delay_sec_;
    int connection_timeout_sec_;
    std::string reconnect_delay_sec_text_;
    std::string connection_timeout_sec_text_;
    bool tls_verify_peer_;
    bool tls_use_custom_cert_;
    std::string tls_cert_pin_;

    // === Notification Settings ===
    bool desktop_notifications_;
    bool sound_alerts_;
    bool notify_on_mention_;
    bool notify_on_dm_;
    std::string mention_keywords_;

    // === Privacy Settings ===
    bool share_typing_indicators_ = true;
    bool share_read_receipts_ = true;
    bool auto_away_enabled_ = false;
    std::string auto_away_minutes_ = "10";

    // === Account Settings ===
    std::string nickname_;
    std::string public_key_hex_;

    // Components
    ftxui::Component container_;
    ftxui::Component sidebar_container_;
    ftxui::Component content_container_;
    ftxui::Component general_container_;
    ftxui::Component appearance_container_;
    ftxui::Component voice_container_;
    ftxui::Component connection_container_;
    ftxui::Component notifications_container_;
    ftxui::Component privacy_container_;
    ftxui::Component account_container_;
    ftxui::Component actions_container_;
    ftxui::Component save_button_;
    ftxui::Component cancel_button_;
    ftxui::Component reset_button_;
    ftxui::Component export_button_;
    ftxui::Component import_button_;
    ftxui::Component logout_button_;
    
    // Input components for each category
    ftxui::Component theme_toggle_;
    ftxui::Component timestamp_format_input_;
    ftxui::Component max_messages_input_;
    ftxui::Component image_columns_input_;
    ftxui::Component image_rows_input_;
    ftxui::Component terminal_graphics_toggle_;
    ftxui::Component voice_input_device_dropdown_;
    ftxui::Component voice_output_device_dropdown_;
    ftxui::Component voice_mode_dropdown_;
    ftxui::Component voice_capture_key_button_;
    ftxui::Component voice_noise_suppression_cb_;
    ftxui::Component voice_noise_suppression_level_dropdown_;
    ftxui::Component voice_vad_threshold_slider_;
    ftxui::Component voice_jitter_buffer_slider_;
    ftxui::Component voice_limiter_cb_;
    ftxui::Component voice_limiter_threshold_slider_;
    ftxui::Component voice_input_volume_slider_;
    ftxui::Component voice_output_volume_slider_;
    ftxui::Component voice_self_test_button_;
    ftxui::Component reconnect_delay_input_;
    ftxui::Component timeout_input_;
    ftxui::Component cert_pin_input_;
    ftxui::Component keywords_input_;
    ftxui::Component nickname_input_;
    ftxui::Component language_toggle_;

    // Checkbox components (must persist across renders)
    ftxui::Component show_timestamps_cb_;
    ftxui::Component show_user_colors_cb_;
    ftxui::Component copy_selection_on_release_cb_;
    ftxui::Component inline_images_cb_;
    ftxui::Component auto_reconnect_cb_;
    ftxui::Component tls_verify_cb_;
    ftxui::Component desktop_notif_cb_;
    ftxui::Component sound_alerts_cb_;
    ftxui::Component mention_cb_;
    ftxui::Component dm_cb_;
    ftxui::Component share_typing_indicators_cb_;
    ftxui::Component share_read_receipts_cb_;
    ftxui::Component auto_away_cb_;
    ftxui::Component auto_away_minutes_input_;

    // Account action buttons (must persist across renders)
    ftxui::Component export_button_persistent_;
    ftxui::Component import_button_persistent_;
    ftxui::Component logout_button_persistent_;
};

// Get available theme names
const std::vector<std::string>& available_themes();

// Apply theme by name (updates color scheme)
void apply_theme(const std::string& theme_name);

} // namespace grotto::ui
