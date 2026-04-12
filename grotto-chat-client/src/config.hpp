#pragma once
#include <string>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace grotto {

struct ServerConfig {
    std::string host = "localhost";
    uint16_t    port = 6667;
    std::string cert_pin;   // SHA-256 hex fingerprint for cert pinning
};

struct IdentityConfig {
    std::string user_id  = "user";
    std::string key_file;   // empty = default platform path
    std::string password;   // for key recovery on identity key mismatch (optional)
};

struct UiConfig {
    std::string theme             = "tokyo-night";
    std::string timestamp_format  = "%H:%M";
    int         max_messages      = 1000;
    int         font_scale        = 100;     // Percentage (100 = 100%)
    bool        show_timestamps   = true;
    bool        show_user_colors  = true;
    bool        copy_selection_on_release = true;
    // User list panel settings (persisted)
    int         user_list_width   = 20;      // Width in columns
    bool        user_list_collapsed = false; // Start collapsed?
    int         files_panel_width = 34;      // Width in columns
    std::string language          = "fi";    // "en" or "fi"
};

struct VoiceConfig {
    std::string input_device;
    std::string output_device;
    int         input_volume  = 100;       // Percentage (100 = unity)
    int         output_volume = 100;       // Percentage (100 = unity)
    int         opus_bitrate  = 64000;
    int         frame_ms      = 20;
    int         jitter_buffer_frames = 4;  // 20 ms Opus frames buffered before playout
    bool        noise_suppression_enabled = true;
    std::string noise_suppression_level = "moderate"; // Legacy load-only field.
    bool        limiter_enabled = true;
    float       limiter_threshold = 0.85f;
    std::string mode          = "toggle";  // "toggle", "hold", or "vox"
    std::string ptt_key       = "§";
    float       vad_threshold = 0.02f;
    // ICE servers (STUN/TURN) — if empty, uses default Google STUN
    std::vector<std::string> ice_servers;
    std::string turn_username;
    std::string turn_password;
};

struct PreviewConfig {
    bool enabled       = true;
    int  fetch_timeout = 5;
    int  max_cache     = 200;
    bool inline_images = true;
    int  image_columns = 40;
    int  image_rows    = 16;
    std::string terminal_graphics = "auto";  // "auto" | "off" | "viewer-only"
};

struct TlsConfig {
    bool verify_peer = true;
};

struct ConnectionConfig {
    bool auto_reconnect      = true;
    int  reconnect_delay_sec = 5;
    int  timeout_sec         = 30;
};

struct NotificationConfig {
    bool        desktop_notifications = true;
    bool        sound_alerts          = true;
    bool        notify_on_mention     = true;
    bool        notify_on_dm          = true;
    std::string mention_keywords;          // Comma-separated list
};

struct PrivacyConfig {
    bool        share_typing_indicators = true;
    bool        share_read_receipts = true;
    bool        auto_away_enabled = false;
    int         auto_away_minutes = 10;
};

struct SessionConfig {
    std::vector<std::string> remembered_channels;
    std::vector<std::string> remembered_direct_messages;
};

struct ClientConfig {
    ServerConfig       server;
    IdentityConfig     identity;
    UiConfig           ui;
    VoiceConfig        voice;
    PreviewConfig      preview;
    TlsConfig          tls;
    ConnectionConfig   connection;
    NotificationConfig notifications;
    PrivacyConfig      privacy;
    SessionConfig      session;

    // Derived: platform-specific config directory
    std::filesystem::path config_dir;
    std::filesystem::path db_path;
};

// Load config from TOML file. Returns default config on failure.
ClientConfig load_config(const std::filesystem::path& path);

// Save config back to TOML file (creates file if missing).
void save_config(const ClientConfig& cfg, const std::filesystem::path& path);

// Returns the default platform config directory:
//   Windows: %APPDATA%\\grotto
//   Linux:   ~/.config/grotto/
std::filesystem::path default_config_dir();

// Returns the default path for remembered login credentials.
std::filesystem::path default_credentials_path();

// Returns the remembered-credentials path under a specific config directory.
std::filesystem::path credentials_path_for_config_dir(const std::filesystem::path& config_dir);

// Clears remembered credentials and local encrypted client state.
bool clear_local_client_state(ClientConfig& cfg,
                              const std::filesystem::path& config_path,
                              std::string* status_message = nullptr);

// Export settings to a backup file
void export_settings(const ClientConfig& cfg, const std::filesystem::path& path);

// Import settings from a backup file (returns true on success)
bool import_settings(ClientConfig& cfg, const std::filesystem::path& path);

} // namespace grotto
