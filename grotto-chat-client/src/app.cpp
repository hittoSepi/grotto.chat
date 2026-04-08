#include "app.hpp"
#include "input/command_parser.hpp"
#include "ui/login_screen.hpp"
#include "ui/settings_screen.hpp"
#include "ui/mouse_support.hpp"
#include "ui/terminal_image.hpp"
#include "version.hpp"
#include "i18n/strings.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <sodium.h>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <chrono>
#include <regex>
#include <thread>
#include <functional>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ctime>
#include <optional>
#include <sstream>
#include <string_view>

namespace grotto {

namespace {

constexpr size_t kMaxPlaintextMessageBytes = 4096;
constexpr std::size_t kMaxRememberedTargets = 24;
constexpr auto kTypingIdleTimeout = std::chrono::seconds(3);
constexpr auto kTypingRefreshInterval = std::chrono::seconds(2);
constexpr auto kTypingRemoteTtl = std::chrono::seconds(5);
constexpr int kAutoAwayMinMinutes = 1;
constexpr int kAutoAwayMaxMinutes = 240;

bool is_server_channel(std::string_view channel_id) {
    if (channel_id.size() != 6) {
        return false;
    }

    constexpr std::string_view kServerChannel = "server";
    for (size_t i = 0; i < channel_id.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(channel_id[i]);
        if (static_cast<char>(std::tolower(c)) != kServerChannel[i]) {
            return false;
        }
    }
    return true;
}

std::string canonical_channel_id(std::string channel_id) {
    if (is_server_channel(channel_id)) {
        return "server";
    }
    return channel_id;
}

bool is_channel_target(std::string_view channel_id) {
    return !channel_id.empty() && channel_id.front() == '#';
}

bool is_direct_target(std::string_view channel_id) {
    return !channel_id.empty() && !is_server_channel(channel_id) && !is_channel_target(channel_id);
}

std::string canonical_file_target(std::string_view recipient_id, std::string_view channel_id) {
    if (!channel_id.empty()) {
        return canonical_channel_id(std::string(channel_id));
    }
    if (!recipient_id.empty()) {
        return std::string(recipient_id);
    }
    return {};
}

std::string typing_target_for_active_channel(std::string_view active_channel) {
    if (active_channel.empty() || is_server_channel(active_channel)) {
        return {};
    }
    return std::string(active_channel);
}

std::string ascii_lower_copy_view(std::string_view text) {
    std::string lowered(text);
    for (char& c : lowered) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lowered;
}

std::string trim_ascii_whitespace(std::string_view text) {
    size_t start = 0;
    size_t end = text.size();
    while (start < end &&
           std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }
    while (end > start &&
           std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(start, end - start));
}

bool is_valid_channel_char(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '.';
}

std::optional<std::string> sanitize_channel_name(std::string_view raw_name) {
    std::string channel = trim_ascii_whitespace(raw_name);
    if (channel.empty()) {
        return std::nullopt;
    }

    if (is_server_channel(channel)) {
        return std::string("server");
    }

    if (channel.front() != '#') {
        channel.insert(channel.begin(), '#');
    }

    if (channel.size() == 1) {
        return std::nullopt;
    }

    for (size_t i = 1; i < channel.size(); ++i) {
        if (!is_valid_channel_char(static_cast<unsigned char>(channel[i]))) {
            return std::nullopt;
        }
    }

    return channel;
}

std::string ascii_lower_copy(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

bool mime_rule_matches(std::string_view rule, std::string_view mime_type) {
    if (rule.empty() || mime_type.empty()) {
        return false;
    }
    if (rule.back() == '*') {
        rule.remove_suffix(1);
        return mime_type.substr(0, rule.size()) == rule;
    }
    if (rule.back() == '/') {
        return mime_type.substr(0, rule.size()) == rule;
    }
    return mime_type == rule;
}

bool mime_allowed_by_policy(std::string_view mime_type,
                            const std::vector<std::string>& allowed_mime_types,
                            const std::vector<std::string>& blocked_mime_types) {
    if (!mime_type.empty()) {
        for (const auto& blocked : blocked_mime_types) {
            if (mime_rule_matches(blocked, mime_type)) {
                return false;
            }
        }
    }

    if (allowed_mime_types.empty()) {
        return true;
    }

    if (mime_type.empty()) {
        return false;
    }

    for (const auto& allowed : allowed_mime_types) {
        if (mime_rule_matches(allowed, mime_type)) {
            return true;
        }
    }
    return false;
}

std::string human_bytes(uint64_t bytes) {
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < std::size(kUnits)) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream oss;
    if (unit_index == 0) {
        oss << bytes << ' ' << kUnits[unit_index];
    } else if (value >= 10.0) {
        oss << std::fixed << std::setprecision(0) << value << ' ' << kUnits[unit_index];
    } else {
        oss << std::fixed << std::setprecision(1) << value << ' ' << kUnits[unit_index];
    }
    return oss.str();
}

bool is_quota_error_message(std::string_view message) {
    return message == "User storage quota exceeded" ||
           message == "Server storage quota exceeded";
}

std::string format_presence_since_time(int64_t status_since_ms) {
    if (status_since_ms <= 0) {
        return {};
    }

    const std::time_t raw_time = static_cast<std::time_t>(status_since_ms / 1000);
    std::tm local_tm{};
#ifdef _WIN32
    if (localtime_s(&local_tm, &raw_time) != 0) {
        return {};
    }
#else
    if (localtime_r(&raw_time, &local_tm) == nullptr) {
        return {};
    }
#endif

    char buffer[16]{};
    if (std::strftime(buffer, sizeof(buffer), "%H:%M", &local_tm) == 0) {
        return {};
    }
    return std::string(buffer);
}

std::string dm_presence_notice(const PresenceInfo& presence, std::string_view user_id) {
    if (presence.status != PresenceStatus::Away && presence.status != PresenceStatus::Dnd) {
        return {};
    }

    std::string notice = std::string(user_id);
    if (presence.status == PresenceStatus::Away) {
        notice += " is away right now";
    } else {
        notice += " is in do not disturb right now";
    }

    if (!presence.status_text.empty()) {
        notice += ": '" + presence.status_text + "'";
    }

    const std::string since = format_presence_since_time(presence.status_since_ms);
    if (!since.empty()) {
        notice += " (" + since + ")";
    }
    return notice;
}

std::string dm_presence_notice_key(const PresenceInfo& presence) {
    if (presence.status != PresenceStatus::Away && presence.status != PresenceStatus::Dnd) {
        return {};
    }
    return std::to_string(static_cast<int>(presence.status)) + "|" +
           presence.status_text + "|" + std::to_string(presence.status_since_ms);
}

std::string quota_preflight_message(std::string_view label, uint64_t file_size, uint64_t limit) {
    return "Upload blocked: file exceeds the " + std::string(label) + " (" +
           human_bytes(file_size) + " > " + human_bytes(limit) + ")";
}

std::chrono::minutes auto_away_timeout(const ClientConfig& cfg) {
    return std::chrono::minutes(
        std::clamp(cfg.privacy.auto_away_minutes, kAutoAwayMinMinutes, kAutoAwayMaxMinutes));
}

std::filesystem::path default_download_path_for_file(std::string_view file_id,
                                                     std::string_view filename) {
    std::filesystem::path base = "downloads";
    std::string preferred = filename.empty() ? std::string(file_id) : std::string(filename);
    if (preferred.empty()) {
        preferred = "download.bin";
    }

    std::filesystem::path candidate = base / preferred;
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }

    const auto stem = candidate.stem().string();
    const auto ext = candidate.extension().string();
    for (int i = 2; i < 1000; ++i) {
        auto next = base / (stem + " (" + std::to_string(i) + ")" + ext);
        if (!std::filesystem::exists(next)) {
            return next;
        }
    }
    return base / (std::string(file_id) + ext);
}

std::filesystem::path default_downloads_dir() {
    return std::filesystem::path("downloads");
}

std::string generate_message_id() {
    std::array<unsigned char, 16> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(bytes.size() * 2, '\0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        out[i * 2] = kHex[(bytes[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[bytes[i] & 0x0F];
    }
    return out;
}

std::string transfer_state_label(client::file::TransferState state) {
    using client::file::TransferState;
    switch (state) {
        case TransferState::PENDING: return "queued";
        case TransferState::UPLOADING: return "uploading";
        case TransferState::DOWNLOADING: return "downloading";
        case TransferState::COMPLETED: return "completed";
        case TransferState::FAILED: return "failed";
        case TransferState::CANCELLED: return "cancelled";
    }
    return "unknown";
}

std::string transfer_direction_label(client::file::TransferDirection direction) {
    return direction == client::file::TransferDirection::UPLOAD ? "upload" : "download";
}

std::string transfer_target_label(const client::file::TransferInfo& info) {
    if (!info.channel_id.empty()) {
        return info.channel_id;
    }
    if (!info.recipient_id.empty()) {
        return "dm:" + info.recipient_id;
    }
    if (!info.local_path.empty()) {
        return info.local_path.string();
    }
    return "-";
}

std::string transfer_progress_label(const client::file::TransferInfo& info) {
    std::ostringstream oss;
    const auto progress = std::clamp(info.progress, 0.0f, 1.0f);
    const auto percent = static_cast<int>(std::lround(progress * 100.0f));
    oss << percent << "%";
    if (info.file_size > 0) {
        oss << " (" << human_bytes(info.bytes_transferred)
            << " / " << human_bytes(info.file_size) << ")";
    } else if (info.bytes_transferred > 0) {
        oss << " (" << human_bytes(info.bytes_transferred) << ")";
    }
    return oss.str();
}

}  // namespace

App::App() = default;

App::~App() {
    if (ui_) {
        save_current_config();
    }
    ioc_.stop();
    if (io_thread_.joinable()) io_thread_.join();
    if (previewer_) previewer_->stop();
}

bool App::init(const std::filesystem::path& config_path,
               const std::string& user_id_override,
               const std::optional<std::pair<std::string, uint16_t>>& server_url) {
    config_path_ = config_path;
    
    // ── Logging ───────────────────────────────────────────────────────────
    // File sink for debug output (TUI hides stderr)
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "grotto-debug.log", /*truncate=*/true);
    auto logger = std::make_shared<spdlog::logger>("grotto", file_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);

    // ── libsodium ─────────────────────────────────────────────────────────
    if (sodium_init() < 0) {
        std::cerr << "sodium_init() failed\n";
        return false;
    }

    // ── Config ────────────────────────────────────────────────────────────
    cfg_ = load_config(config_path);
    if (!user_id_override.empty()) cfg_.identity.user_id = user_id_override;
    std::filesystem::create_directories(cfg_.config_dir);
    i18n::set_language(cfg_.ui.language);
    refresh_runtime_capabilities();

    // ── Login Screen (FTXUI) ──────────────────────────────────────────────
    // Show the graphical login screen to get credentials
    ui::LoginCredentials login_creds;
    std::optional<ui::LoginCredentials> login_prefill;
    if (server_url) {
        ui::LoginCredentials prefill;
        prefill.host = server_url->first;
        prefill.port = server_url->second;
        login_prefill = prefill;
        spdlog::info("Pre-filled server from URL: {}:{}", prefill.host, prefill.port);
    }

    std::string login_status;
    bool login_status_is_error = true;

    while (true) {
        ui::LoginScreen login_screen;
        ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

        auto result = login_screen.show(
            cfg_, screen, login_creds, login_prefill, login_status, login_status_is_error);
        
        if (result == ui::LoginResult::Cancelled) {
            spdlog::info("Login cancelled by user");
            return false;
        }

        if (result == ui::LoginResult::ClearLocalData) {
            std::string clear_status;
            bool clear_ok = clear_local_client_state(cfg_, config_path, &clear_status);
            login_status = clear_status;
            login_status_is_error = !clear_ok;
            login_prefill.reset();
            continue;
        }

        // Update config with login credentials
        cfg_.server.host = login_creds.host;
        cfg_.server.port = login_creds.port;
        cfg_.identity.user_id = login_creds.username;

        // Save config with new values
        save_config(cfg_, config_path);

        try {
            store_ = std::make_unique<db::LocalStore>(cfg_.db_path);
        } catch (const std::exception& e) {
            login_status = i18n::tr(i18n::I18nKey::FAILED_OPEN_DATA_STORE, std::string(e.what()));
            login_status_is_error = true;
            store_.reset();
            continue;
        }

        crypto_ = std::make_unique<crypto::CryptoEngine>();
        const std::string& passphrase = login_creds.passkey;

        if (!crypto_->init(*store_, cfg_, passphrase)) {
            crypto_.reset();
            store_.reset();
            login_status = i18n::tr(i18n::I18nKey::FAILED_UNLOCK_IDENTITY);
            login_status_is_error = true;
            continue;
        }

        break;
    }

    // ── Database ──────────────────────────────────────────────────────────
    try {
        store_ = std::make_unique<db::LocalStore>(cfg_.db_path);
    } catch (const std::exception& e) {
        std::cerr << "DB open failed: " << e.what() << "\n";
        return false;
    }

    // ── Crypto (use passkey from login screen) ───────────────────────────
    crypto_ = std::make_unique<crypto::CryptoEngine>();

    // Use the passkey entered in the login screen
    const std::string& passphrase = login_creds.passkey;

    if (!crypto_->init(*store_, cfg_, passphrase)) {
        // Show error and fail - the user will need to restart
        std::cerr << "Crypto init failed (wrong passphrase?)\n";
        return false;
    }

    // ── State & UI ────────────────────────────────────────────────────────
    state_.set_local_user_id(cfg_.identity.user_id);
    state_.ensure_channel("server");
    restore_remembered_targets();

    // Wire AppState::post_ui to also wake the FTXUI event loop
    ui_ = std::make_unique<ui::UIManager>(state_, cfg_);
    // We'll wire notify after ui_ is created — AppState doesn't own the callback;
    // instead UIManager::notify() is called explicitly where needed.

    // ── Voice ─────────────────────────────────────────────────────────────
    voice_ = std::make_unique<voice::VoiceEngine>(state_, cfg_);

    // ── Help system ───────────────────────────────────────────────────────
    {
        std::error_code ec;
        auto bin_dir = std::filesystem::canonical(
            std::filesystem::path(config_path_).parent_path(), ec);
        if (!ec) {
            help_ = std::make_unique<HelpManager>(bin_dir, cfg_.ui.language);
            help_->load();
        } else {
            spdlog::warn("Could not resolve binary directory for help system: {}", ec.message());
        }
    }

    // ── Link Previewer ────────────────────────────────────────────────────
    if (cfg_.preview.enabled) {
        previewer_ = std::make_unique<LinkPreviewer>(
            *store_, cfg_.preview.fetch_timeout, cfg_.preview.max_cache,
            cfg_.preview.inline_images, cfg_.preview.image_columns, cfg_.preview.image_rows);
        previewer_->start();
    }

    // ── Network ───────────────────────────────────────────────────────────
    // Use the login passkey as the default recovery secret unless config overrides it.
    if (cfg_.identity.password.empty()) {
        cfg_.identity.password = login_creds.passkey;
    }
    msg_handler_ = std::make_unique<net::MessageHandler>(state_, *crypto_, cfg_);

    net::NetCallbacks cb;
    cb.on_message = [this](const Envelope& env) {
        msg_handler_->dispatch(env);
        // Wake UI for any received message
        if (ui_) ui_->notify();
    };
    cb.on_trace = [this](const std::string& text, bool reset_attempt_timer, bool activate_server) {
        trace_connection_phase(text, reset_attempt_timer, activate_server);
    };
    cb.on_connected = [this]() {
        // Send HELLO
        Hello hello;
        hello.set_protocol_version(1);
        hello.set_client_version(std::string("grotto-client/") + std::string(grotto::VERSION));
        Envelope env;
        env.set_seq(1);
        env.set_type(MT_HELLO);
        env.set_payload(hello.SerializeAsString());
        net_client_->send(env);
        trace_connection_phase("HELLO sent", false, true);
    };
    cb.on_disconnected = [this](const std::string& reason) {
        state_.set_connected(false);
        state_.set_connecting(false);
        msg_handler_->on_transport_disconnected();
        crypto_->reset_group_sessions();  // Re-send SKDM on next connection
        clear_pending_channel_commands();
        stop_local_typing();
        {
            std::lock_guard lk(typing_mu_);
            remote_typing_.clear();
        }
        {
            std::lock_guard lk(pending_read_receipts_mu_);
            pending_read_receipts_.clear();
        }
        log_server_event("Disconnected: " + reason, true);
    };

    net_client_ = std::make_shared<net::NetClient>(ioc_, cfg_, std::move(cb));
    msg_handler_->set_net_client(net_client_.get());
    msg_handler_->set_voice_engine(voice_.get());
    msg_handler_->set_persist_callback([this](const std::string& channel_id, const Message& msg) {
        persist_message(channel_id, msg);
    });
    msg_handler_->set_trace_callback([this](const std::string& text) {
        trace_connection_phase(text, false, true);
    });
    msg_handler_->set_command_response_callback([this](const CommandResponse& response) {
        handle_command_response(response);
    });
    msg_handler_->set_file_policy_callback([this](const FileTransferPolicy& policy) {
        update_file_transfer_policy(policy);
    });
    msg_handler_->set_file_list_callback([this](const FileListResponse& response) {
        handle_file_list_response(response);
    });
    msg_handler_->set_file_changed_callback([this](const FileChanged& changed) {
        handle_file_changed(changed);
    });
    msg_handler_->set_file_error_callback([this](const FileError& error) {
        return handle_file_transfer_error(error);
    });
    msg_handler_->set_typing_callback([this](const TypingUpdate& typing) {
        handle_typing_update(typing);
    });
    msg_handler_->set_read_receipt_callback([this](const ReadReceipt& receipt) {
        handle_read_receipt(receipt);
    });
    msg_handler_->set_dm_read_candidate_callback([this](const std::string& channel_id, const Message& msg) {
        track_dm_read_candidate(channel_id, msg);
    });
    msg_handler_->set_preview_callback([this](const std::string& channel_id, const std::string& text) {
        trigger_previews(channel_id, text);
    });

    file_mgr_ = std::make_unique<client::file::FileTransferManager>();
    file_mgr_->set_send_function([this](uint32_t msg_type, const google::protobuf::Message& msg) {
        if (!net_client_) return;
        Envelope env;
        env.set_type(static_cast<MessageType>(msg_type));
        env.set_payload(msg.SerializeAsString());
        env.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        net_client_->send(env);
    });
    ui_->set_transfer_summary_provider([this]() {
        return build_transfer_summary();
    });
    ui_->set_quota_summary_provider([this]() {
        return files_panel_quota_summary();
    });
    ui_->set_typing_summary_provider([this]() {
        return build_typing_summary();
    });
    ui_->set_quota_refresh_handler([this]() {
        request_quota_summary();
    });
    ui_->set_files_refresh_handler([this](const std::string& target) {
        request_remote_files_for_target(target, false);
    });
    ui_->set_file_download_handler([this](const RemoteFileEntry& file) {
        download_remote_file(file);
    });
    ui_->set_file_delete_handler([this](const RemoteFileEntry& file) {
        delete_remote_file(file);
    });
    msg_handler_->set_file_transfer_manager(file_mgr_.get());

    voice_->set_send_signal([this](const VoiceSignal& sig) {
        Envelope env;
        env.set_type(MT_VOICE_SIGNAL);
        env.set_payload(sig.SerializeAsString());
        net_client_->send(env);
    });

    voice_->set_send_room_msg([this](MessageType type, const google::protobuf::Message& msg) {
        Envelope env;
        env.set_type(type);
        msg.SerializeToString(env.mutable_payload());
        net_client_->send(env);
    });

    return true;
}

int App::run() {
    state_.set_connecting(true);
    last_local_activity_ = std::chrono::steady_clock::now();
    schedule_auto_away();

    // ── IO thread ─────────────────────────────────────────────────────────
    auto work = boost::asio::make_work_guard(ioc_);
    io_thread_ = std::thread([this]() {
        net_client_->start();
        ioc_.run();
    });

    // ── Speaking indicator refresh timer (100ms) ──────────────────────────
    auto speaking_timer = std::make_shared<boost::asio::steady_timer>(ioc_);
    std::function<void()> refresh_speaking;
    refresh_speaking = [this, speaking_timer, &refresh_speaking]() {
        voice_->refresh_speaking_state();
        speaking_timer->expires_after(std::chrono::milliseconds(100));
        speaking_timer->async_wait([&refresh_speaking](auto) { refresh_speaking(); });
    };
    speaking_timer->expires_after(std::chrono::milliseconds(100));
    speaking_timer->async_wait([&refresh_speaking](auto) { refresh_speaking(); });

    ui_->push_system_msg(i18n::tr(i18n::I18nKey::GROTTO_CONNECTING,
                                   std::string(grotto::VERSION),
                                   cfg_.server.host,
                                   std::to_string(cfg_.server.port)));

    // ── FTXUI event loop (blocks main thread) ─────────────────────────────
    ui_->run(
        [this](const std::string& line) { on_submit(line); },
        [this](const std::string& text) { on_input_changed(text); },
        [this]()                         { ioc_.stop(); },
        [this](int idx)                  { switch_to_channel_by_index(idx); },
        [this](int delta)                { switch_channel(delta); },
        [this](bool active)              { voice_->set_ptt_active(active); },
        [this]()                         { open_settings(); },
        [this](const std::string& ch)    { on_active_channel_changed(ch); }
    );

    // ── Shutdown ─────────────────────────────────────────────────────────
    ioc_.stop();
    work.reset();
    if (io_thread_.joinable()) io_thread_.join();
    if (previewer_) previewer_->stop();

    return should_exit_ ? 1 : 0;
}

void App::on_submit(const std::string& line) {
    if (line.empty()) return;
    note_local_activity(!line.empty() && line[0] != '/');
    stop_local_typing();

    if (line[0] == '/') {
        auto cmd = parse_command(line);
        if (cmd) handle_command(*cmd);
        else     ui_->push_system_msg(i18n::tr(i18n::I18nKey::UNKNOWN_COMMAND, line));
    } else {
        send_chat(line);
    }
}

void App::on_input_changed(const std::string& text) {
    note_local_activity(!text.empty() && text[0] != '/');
    const auto active = canonical_channel_id(state_.active_channel().value_or(""));
    const std::string target = typing_target_for_active_channel(active);
    if (!cfg_.privacy.share_typing_indicators || target.empty() || text.empty() || text[0] == '/') {
        stop_local_typing();
        return;
    }

    if (!msg_handler_ || !msg_handler_->is_authenticated() || !net_client_ || !net_client_->is_connected()) {
        stop_local_typing();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (local_typing_active_ && local_typing_target_ != target) {
        send_typing_update(local_typing_target_, false);
        local_typing_active_ = false;
        local_typing_target_.clear();
    }

    if (!local_typing_active_ || local_typing_target_ != target ||
        now - last_typing_sent_ >= kTypingRefreshInterval) {
        send_typing_update(target, true);
        local_typing_active_ = true;
        local_typing_target_ = target;
        last_typing_sent_ = now;
    }

    typing_idle_timer_.expires_after(kTypingIdleTimeout);
    typing_idle_timer_.async_wait([this, target](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }
        if (local_typing_active_ && local_typing_target_ == target) {
            stop_local_typing();
        }
    });
}

void App::handle_command(const ParsedCommand& cmd) {
    if (cmd.name == "/quit" || cmd.name == "/exit") {
        // Fully exit: disconnect from server + close UI
        ioc_.stop();
        ui_->request_exit();
        return;
    }
    if (cmd.name == "/disconnect") {
        // Disconnect from server but stay in the UI
        if (net_client_ && net_client_->is_connected()) {
            ioc_.stop();
            msg_handler_->on_transport_disconnected();
            state_.set_connected(false);
            state_.set_connecting(false);
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::DISCONNECTED_FROM_SERVER));
        } else {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::NOT_CONNECTED));
        }
        return;
    }
    if (cmd.name == "/version") {
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::CLIENT_VERSION, std::string(grotto::VERSION)));
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::SERVER_VERSION, server_version_));
        return;
    }
    if (cmd.name == "/status") {
        bool connected = net_client_ && net_client_->is_connected();
        bool authed = msg_handler_ && msg_handler_->is_authenticated();
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::CONNECTION_STATUS,
                                       i18n::tr(connected ? i18n::I18nKey::CONNECTED : i18n::I18nKey::DISCONNECTED)));
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::AUTH_STATUS,
                                       i18n::tr(authed ? i18n::I18nKey::AUTHENTICATED : i18n::I18nKey::NOT_AUTHENTICATED)));
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::USER_LABEL, cfg_.identity.user_id));
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::ACTIVE_CHANNEL,
                                       state_.active_channel().value_or(i18n::tr(i18n::I18nKey::NONE))));
        return;
    }
    if (cmd.name == "/settings") {
        open_settings();
        return;
    }
    if (cmd.name == "/help") {
        if (cmd.args.empty()) {
            auto content = help_ ? help_->get("help") : std::nullopt;
            if (content) {
                ui_->push_system_msg(*content);
            } else {
                // Fallback if help.md not found
                auto topics = help_ ? help_->topics() : std::vector<std::string>{};
                std::string list;
                for (auto& t : topics) { list += t + " "; }
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::HELP_USAGE));
                if (!list.empty())
                    ui_->push_system_msg(i18n::tr(i18n::I18nKey::AVAILABLE_TOPICS, list));
            }
        } else {
            std::string topic = cmd.args[0];
            auto content = help_ ? help_->get(topic) : std::nullopt;
            if (content) {
                ui_->push_system_msg(*content);
            } else {
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::TOPIC_NOT_FOUND, topic));
                auto topics = help_ ? help_->topics() : std::vector<std::string>{};
                std::string list;
                for (auto& t : topics) { list += t + " "; }
                if (!list.empty())
                    ui_->push_system_msg(i18n::tr(i18n::I18nKey::AVAILABLE_TOPICS, list));
            }
        }
        ui_->notify();
        return;
    }
    if (cmd.name == "/reload_help") {
        if (help_) {
            help_->reload();
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::HELP_RELOADED));
        } else {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::HELP_NOT_INITIALIZED));
        }
        ui_->notify();
        return;
    }
    if (cmd.name == "/clear") {
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::CLEARED));
        ui_->notify();
        return;
    }
    if (cmd.name == "/diag") {
        const std::string topic = cmd.args.empty() ? "ui" : ascii_lower_copy(cmd.args[0]);
        if (topic != "ui") {
            ui_->push_system_msg("Usage: /diag ui");
            ui_->notify();
            return;
        }

        ui_->push_system_msg("[diag/ui]");
        ui_->push_system_msg("  copy_selection_on_release=" +
                             std::string(cfg_.ui.copy_selection_on_release ? "true" : "false"));
        ui_->push_system_msg("  clipboard_backend=" + ui::clipboard_backend_name());
        ui_->push_system_msg("  preview.terminal_graphics=" + cfg_.preview.terminal_graphics);
        ui_->push_system_msg("  terminal_protocol_detected=" +
                             terminal_inline_protocol_name(runtime_capabilities_.terminal_protocol_detected));
        ui_->push_system_msg("  compositor_protocol=" +
                             terminal_inline_protocol_name(runtime_capabilities_.compositor_protocol));
        ui_->push_system_msg("  inline_native_enabled=" +
                             std::string(runtime_capabilities_.inline_native_enabled ? "true" : "false"));
        ui_->push_system_msg("  clipboard_available=" +
                             std::string(runtime_capabilities_.clipboard_available ? "true" : "false"));
        ui_->push_system_msg("  audio_input_devices=" +
                             std::to_string(runtime_capabilities_.audio_input_device_count));
        ui_->push_system_msg("  audio_output_devices=" +
                             std::to_string(runtime_capabilities_.audio_output_device_count));
        ui_->push_system_msg("  audio_capture_available=" +
                             std::string(runtime_capabilities_.audio_capture_available ? "true" : "false"));
        ui_->push_system_msg("  audio_playback_available=" +
                             std::string(runtime_capabilities_.audio_playback_available ? "true" : "false"));
        ui_->notify();
        return;
    }
    if (cmd.name == "/names") {
        auto users = state_.online_users();
        std::string list;
        for (auto& u : users) list += u + " ";
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::ONLINE_USERS,
                                       list.empty() ? i18n::tr(i18n::I18nKey::NONE) : list));
        ui_->notify();
        return;
    }
    if (cmd.name == "/search" && !cmd.args.empty()) {
        std::string query;
        for (size_t i = 0; i < cmd.args.size(); ++i) {
            if (i > 0) query += " ";
            query += cmd.args[i];
        }
        if (store_) {
            db::LocalStore::SearchFilters filters;
            filters.limit = 20;
            auto results = store_->search_messages(query, filters);
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::SEARCH_RESULTS, query));
            if (results.empty()) {
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::NO_RESULTS));
            } else {
                for (const auto& r : results) {
                    std::string preview = r.content;
                    if (preview.length() > 60) preview = preview.substr(0, 57) + "...";
                    ui_->push_system_msg("  [" + r.channel_id + "] <" + r.sender_id + ">: " + preview);
                }
            }
        } else {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::SEARCH_NOT_AVAILABLE));
        }
        ui_->notify();
        return;
    }
    // All remaining commands require server connection + authentication
    if (!msg_handler_ || !msg_handler_->is_authenticated()) {
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::NOT_CONNECTED_CHECK_STATUS));
        ui_->notify();
        return;
    }

    if (cmd.name == "/away" || cmd.name == "/back" || cmd.name == "/dnd") {
        auto_away_active_ = false;
        if (cmd.name == "/back") {
            schedule_auto_away();
        } else {
            cancel_auto_away();
        }
        const std::string command = cmd.name.substr(1);
        msg_handler_->send_command(command, cmd.args);
        return;
    }

    if (cmd.name == "/part") {
        auto ch = state_.active_channel().value_or("");
        if (ch.empty() || is_server_channel(ch)) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::CANNOT_LEAVE_SERVER_CHANNEL));
        } else {
            {
                std::lock_guard lk(pending_command_mu_);
                if (has_pending_command(pending_parts_, ch)) {
                    log_server_event("part already pending for " + ch, true);
                    ui_->notify();
                    return;
                }
                pending_parts_.push_back(ch);
            }
            msg_handler_->send_command("part", {ch});
            log_server_event("part requested for " + ch, false);
        }
    } else if (cmd.name == "/join" && !cmd.args.empty()) {
        auto target = sanitize_channel_name(cmd.args[0]);
        if (!target) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::INVALID_CHANNEL_NAME));
            return;
        }
        if (is_server_channel(*target)) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::SERVER_RESERVED_TAB));
            return;
        }
        {
            std::lock_guard lk(pending_command_mu_);
            if (has_pending_command(pending_joins_, *target)) {
                log_server_event("join already pending for " + *target, true);
                ui_->notify();
                return;
            }
            pending_joins_.push_back(*target);
        }
        msg_handler_->send_command("join", {*target});
        log_server_event("join requested for " + *target, false);
    } else if (cmd.name == "/msg" && cmd.args.size() >= 2) {
        std::string target = cmd.args[0];
        std::string text;
        for (size_t i = 1; i < cmd.args.size(); ++i) {
            if (i > 1) text += ' ';
            text += cmd.args[i];
        }
        state_.ensure_channel(target);
        state_.set_active_channel(target);
        remember_target(target);
        send_chat(text);
    } else if (cmd.name == "/call" && !cmd.args.empty()) {
        voice_->call(cmd.args[0]);
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::CALLING, cmd.args[0]));
    } else if (cmd.name == "/accept" && !cmd.args.empty()) {
        voice_->accept_call(cmd.args[0]);
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::ACCEPTED_CALL, cmd.args[0]));
    } else if (cmd.name == "/hangup") {
        voice_->hangup();
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::CALL_ENDED));
    } else if (cmd.name == "/voice") {
        if (!cmd.args.empty() && cmd.args[0] == "leave") {
            voice_->leave_room();
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::LEFT_VOICE_ROOM));
        } else if (voice_->in_voice()) {
            voice_->leave_room();
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::LEFT_VOICE_ROOM));
        } else {
            std::string ch = cmd.args.empty()
                ? state_.active_channel().value_or("#general")
                : cmd.args[0];
            if (is_server_channel(ch)) {
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::SERVER_RESERVED_TAB));
                return;
            }
            voice_->join_room(ch);
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::JOINING_VOICE_ROOM, ch));
        }
    } else if (cmd.name == "/mute") {
        bool m = !state_.voice_snapshot().muted;
        voice_->set_muted(m);
        ui_->push_system_msg(i18n::tr(m ? i18n::I18nKey::MUTED : i18n::I18nKey::UNMUTED));
    } else if (cmd.name == "/deafen") {
        bool d = !state_.voice_snapshot().deafened;
        voice_->set_deafened(d);
        ui_->push_system_msg(i18n::tr(d ? i18n::I18nKey::DEAFENED : i18n::I18nKey::UNDEAFENED));
    } else if (cmd.name == "/vmode" || cmd.name == "/ptt") {
        voice_->toggle_voice_mode();
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::VOICE_MODE, voice_->voice_mode()));
    } else if (cmd.name == "/transfers") {
        if (!file_mgr_) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::FILE_TRANSFER_NOT_AVAILABLE));
            return;
        }

        std::size_t limit = 8;
        if (!cmd.args.empty()) {
            try {
                limit = std::stoul(cmd.args[0]);
                if (limit == 0) {
                    limit = 8;
                }
            } catch (...) {
                ui_->push_system_msg("Usage: /transfers [limit]");
                return;
            }
        }

        const auto lines = format_transfer_lines(limit);
        if (lines.empty()) {
            ui_->push_system_msg("No file transfers yet.");
            return;
        }
        for (const auto& line : lines) {
            ui_->push_system_msg(line);
        }
    } else if (cmd.name == "/files") {
        const auto active = canonical_channel_id(state_.active_channel().value_or(""));
        if (active.empty() || is_server_channel(active)) {
            ui_->push_system_msg("Open a channel or DM before listing files.");
            return;
        }
        request_remote_files_for_target(active, true);
        if (ui_) {
            ui_->show_files_panel();
        }
    } else if (cmd.name == "/downloads") {
        const auto downloads_dir = default_downloads_dir();
        std::filesystem::create_directories(downloads_dir);
        ui::open_path(downloads_dir.string());
        ui_->push_system_msg("Opened downloads folder: " + downloads_dir.string());
    } else if (cmd.name == "/rmfile") {
        if (cmd.args.empty()) {
            ui_->push_system_msg("Usage: /rmfile <file-id>");
            return;
        }
        msg_handler_->send_command("rmfile", {cmd.args[0]});
        log_server_event("file delete requested: " + cmd.args[0], false);
    } else if (cmd.name == "/quota") {
        msg_handler_->send_command("quota", {});
        log_server_event("quota requested", false);
    } else if (cmd.name == "/upload") {
        if (cmd.args.empty()) {
            ui_->push_system_msg("Usage: /upload <local-file-path>");
            return;
        }
        std::filesystem::path file_path = cmd.args[0];
        if (!std::filesystem::exists(file_path)) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::FILE_NOT_FOUND, file_path.string()));
        } else if (!file_mgr_) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::FILE_TRANSFER_NOT_AVAILABLE));
        } else {
            const auto file_size = std::filesystem::file_size(file_path);
            const auto mime_type = ascii_lower_copy_view(client::file::detect_mime_type(file_path));
            if (file_transfer_policy_.received &&
                file_transfer_policy_.max_upload_bytes > 0 &&
                file_size > file_transfer_policy_.max_upload_bytes) {
                ui_->push_system_msg(quota_preflight_message(
                    "upload limit",
                    file_size,
                    file_transfer_policy_.max_upload_bytes));
                return;
            }
            if (file_transfer_policy_.received &&
                file_transfer_policy_.max_total_storage_bytes > 0 &&
                file_size > file_transfer_policy_.max_total_storage_bytes) {
                ui_->push_system_msg(quota_preflight_message(
                    "server storage quota",
                    file_size,
                    file_transfer_policy_.max_total_storage_bytes));
                return;
            }
            if (file_transfer_policy_.received &&
                file_transfer_policy_.max_user_storage_bytes > 0 &&
                file_size > file_transfer_policy_.max_user_storage_bytes) {
                ui_->push_system_msg(quota_preflight_message(
                    "your storage quota",
                    file_size,
                    file_transfer_policy_.max_user_storage_bytes));
                return;
            }
            if (file_transfer_policy_.received &&
                !mime_allowed_by_policy(mime_type,
                                        file_transfer_policy_.allowed_mime_types,
                                        file_transfer_policy_.blocked_mime_types)) {
                ui_->push_system_msg(
                    "Upload blocked by server policy: MIME type not allowed (" + mime_type + ")");
                return;
            }

            const auto active = canonical_channel_id(state_.active_channel().value_or(""));
            if (active.empty() || is_server_channel(active)) {
                ui_->push_system_msg("Open a channel or DM before uploading.");
                return;
            }

            const std::string recipient = is_direct_target(active) ? active : "";
            const std::string channel = is_channel_target(active) ? active : "";
            auto tid = file_mgr_->upload(file_path, recipient, channel);
            if (tid.empty()) {
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::UPLOAD_FAILED));
            } else {
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::UPLOADING,
                                               file_path.filename().string(),
                                               std::to_string(file_size)));
                ui_->push_system_msg("Upload queued for " + active + " (transfer " + tid + ", see /transfers)");
            }
        }
    } else if (cmd.name == "/download") {
        if (cmd.args.empty()) {
            ui_->push_system_msg("Usage: /download <file-id> [save-path]");
            return;
        }
        std::string file_id = cmd.args[0];
        std::filesystem::path save_path = cmd.args.size() > 1
            ? std::filesystem::path(cmd.args[1])
            : default_download_path_for_file(file_id, file_id);
        if (!file_mgr_) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::FILE_TRANSFER_NOT_AVAILABLE));
        } else {
            const auto parent = save_path.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }
            auto tid = file_mgr_->download(file_id, save_path);
            if (tid.empty()) {
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::DOWNLOAD_FAILED));
            } else {
                ui_->push_system_msg(i18n::tr(i18n::I18nKey::DOWNLOADING, file_id, save_path.string()));
                ui_->push_system_msg("Download queued as transfer " + tid + " (see /transfers)");
            }
        }
    } else if (cmd.name == "/trust" && !cmd.args.empty()) {
        std::string peer = cmd.args[0];
        std::string sn   = crypto_->safety_number(peer, *store_);
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::SAFETY_NUMBER_WITH, peer));
        ui_->push_system_msg("  " + sn);
        auto existing = store_->load_peer_identity(peer);
        if (existing) store_->save_peer_identity(peer, *existing, "verified");
    } else if (cmd.name == "/me" && !cmd.args.empty()) {
        // Action message (/me eats pizza)
        std::vector<std::string> action_args;
        std::string action;
        for (size_t i = 0; i < cmd.args.size(); ++i) {
            if (i > 0) action += ' ';
            action += cmd.args[i];
        }
        action_args.push_back(action);
        auto ch = state_.active_channel().value_or("#general");
        // Send action as IRC command
        if (msg_handler_) {
            msg_handler_->send_command("me", action_args);
        }
        // Show locally too
        const std::string& local_id = cfg_.identity.user_id;
        Message action_msg;
        action_msg.sender_id    = local_id;
        action_msg.content      = "* " + local_id + " " + action;
        action_msg.type         = Message::Type::System;
        action_msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_.push_message(ch, std::move(action_msg));
        ui_->notify();
    } else {
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::UNKNOWN_COMMAND, cmd.name));
    }
    ui_->notify();
}

void App::send_chat(const std::string& text) {
    if (!msg_handler_->is_authenticated()) {
        if (net_client_->is_connected()) {
            log_server_event("message blocked: authentication still in progress", true);
        } else {
            log_server_event("message blocked: not connected", true);
        }
        return;
    }
    auto active = canonical_channel_id(state_.active_channel().value_or(""));
    if (active.empty()) {
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::NO_ACTIVE_CHANNEL));
        return;
    }
    if (is_server_channel(active)) {
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::CANNOT_SEND_MESSAGES_HERE));
        return;
    }
    if (text.size() > kMaxPlaintextMessageBytes) {
        ui_->push_system_msg(i18n::tr(
            i18n::I18nKey::MESSAGE_TOO_LONG,
            std::to_string(text.size()),
            std::to_string(kMaxPlaintextMessageBytes)));
        return;
    }

    const std::string& local_id = cfg_.identity.user_id;
    const std::string message_id = generate_message_id();

    if (is_direct_target(active)) {
        const auto peer_presence = state_.presence_info(active);
        if (peer_presence.status == PresenceStatus::Away || peer_presence.status == PresenceStatus::Dnd) {
            const std::string notice_key = dm_presence_notice_key(peer_presence);
            const auto last_notice = dm_presence_notice_keys_.find(active);
            if (last_notice == dm_presence_notice_keys_.end() || last_notice->second != notice_key) {
                const std::string notice = dm_presence_notice(peer_presence, active);
                if (!notice.empty()) {
                    log_system_message(active, notice, false);
                }
                dm_presence_notice_keys_[active] = notice_key;
            }
        } else {
            dm_presence_notice_keys_.erase(active);
        }
    }

    // Show immediately in local UI
    Message local_msg;
    local_msg.message_id   = message_id;
    local_msg.sender_id    = local_id;
    local_msg.content      = text;
    local_msg.type         = Message::Type::Chat;
    local_msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    state_.push_message(active, local_msg);
    ui_->notify();

    // Persist to database for search
    if (store_) {
        db::LocalStore::MessageRow row;
        row.channel_id   = active;
        row.sender_id    = local_id;
        row.content      = text;
        row.timestamp_ms = local_msg.timestamp_ms;
        row.type         = static_cast<int>(Message::Type::Chat);
        store_->save_message(row);
    }

    // Encrypt and send (for DMs with no session yet, msg_handler queues
    // the plaintext and sends KEY_REQUEST; the message is flushed when
    // KEY_BUNDLE arrives in handle_key_bundle).
    auto env = crypto_->encrypt(local_id, active, text,
        [this, text, message_id](const std::string& recipient_id) {
            msg_handler_->request_key(recipient_id, text, message_id);
        });

    if (!env.sender_id().empty()) {
        env.set_message_id(message_id);
        Envelope wire;
        wire.set_type(MT_CHAT_ENVELOPE);
        wire.set_payload(env.SerializeAsString());
        net_client_->send(wire);
    }

    trigger_previews(active, text);
}

void App::trigger_previews(const std::string& channel_id, const std::string& text) {
    if (!previewer_) return;
    static const std::regex url_re(R"(https?://\S{3,})");
    std::sregex_iterator it(text.begin(), text.end(), url_re);
    for (; it != std::sregex_iterator{}; ++it) {
        std::string url = (*it)[0].str();
        spdlog::debug("preview: fetching {}", url);
        const std::string original_text = trim_ascii_whitespace(text);
        previewer_->fetch(url, [this, channel_id, original_text](PreviewResult r) {
            spdlog::debug("preview: result for {} success={} title='{}'", r.url, r.success, r.title);
            if (!r.success) return;
            state_.post_ui([this, channel_id, original_text, r = std::move(r)]() mutable {
                Message pm;
                pm.type      = r.is_image ? Message::Type::Preview : Message::Type::System;
                pm.sender_id = "preview";
                if (r.is_image) {
                    const bool original_is_same_url = (original_text == r.url);
                    if (!original_is_same_url) {
                        pm.content = r.title;
                        pm.render_parts.push_back({MessageRenderPart::Kind::Text, r.title, std::nullopt});
                    }
                    if (!r.description.empty()) {
                        if (!pm.content.empty()) {
                            pm.content += "\n";
                        }
                        pm.content += r.description;
                        pm.render_parts.push_back({MessageRenderPart::Kind::Text, r.description, std::nullopt});
                    }
                    if (!r.thumbnail.rgba.empty()) {
                        pm.inline_image = r.thumbnail;
                        pm.render_parts.push_back({MessageRenderPart::Kind::Image, {}, r.thumbnail});
                    }
                } else {
                    pm.content = "\u250C " + r.title +
                                 (r.description.empty() ? "" : " \u2014 " + r.description);
                }
                pm.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                state_.push_message(channel_id, std::move(pm));
            });
            if (ui_) ui_->notify();
        });
    }
}

// Helper to save incoming messages to database
void App::persist_message(const std::string& channel_id, const Message& msg) {
    if (!store_) return;
    db::LocalStore::MessageRow row;
    row.channel_id   = channel_id;
    row.sender_id    = msg.sender_id;
    row.content      = msg.content;
    row.timestamp_ms = msg.timestamp_ms;
    row.type         = static_cast<int>(msg.type);
    store_->save_message(row);
    if (msg.type == Message::Type::Chat) {
        remember_target(channel_id);
    }
}

void App::restore_remembered_targets() {
    for (const auto& channel : cfg_.session.remembered_channels) {
        const auto target = sanitize_channel_name(channel);
        if (target && !is_server_channel(*target)) {
            state_.ensure_channel(*target);
        }
    }
    for (const auto& dm : cfg_.session.remembered_direct_messages) {
        const std::string target = canonical_channel_id(trim_ascii_whitespace(dm));
        if (!target.empty() && is_direct_target(target)) {
            state_.ensure_channel(target);
            state_.set_direct_message_users(target, cfg_.identity.user_id, target);
        }
    }
}

void App::remember_target(const std::string& channel_id) {
    const std::string target = canonical_channel_id(trim_ascii_whitespace(channel_id));
    if (target.empty() || is_server_channel(target)) {
        return;
    }

    auto remember_into = [](std::vector<std::string>& targets, const std::string& value) {
        targets.erase(std::remove(targets.begin(), targets.end(), value), targets.end());
        targets.push_back(value);
        if (targets.size() > kMaxRememberedTargets) {
            targets.erase(targets.begin(), targets.begin() + static_cast<std::ptrdiff_t>(targets.size() - kMaxRememberedTargets));
        }
    };

    if (is_channel_target(target)) {
        remember_into(cfg_.session.remembered_channels, target);
    } else if (is_direct_target(target)) {
        remember_into(cfg_.session.remembered_direct_messages, target);
    } else {
        return;
    }
    save_current_config();
}

void App::forget_target(const std::string& channel_id) {
    const std::string target = canonical_channel_id(trim_ascii_whitespace(channel_id));
    if (target.empty() || is_server_channel(target)) {
        return;
    }

    auto erase_from = [&target](std::vector<std::string>& targets) {
        targets.erase(std::remove(targets.begin(), targets.end(), target), targets.end());
    };

    erase_from(cfg_.session.remembered_channels);
    erase_from(cfg_.session.remembered_direct_messages);
    save_current_config();
}

void App::log_system_message(const std::string& channel_id,
                             const std::string& text,
                             bool activate_channel) {
    if (!ui_) {
        return;
    }
    ui_->push_system_msg_to_channel(channel_id, text, activate_channel);
}

void App::log_server_event(const std::string& text, bool activate_server) {
    log_system_message("server", text, activate_server);
}

void App::trace_connection_phase(const std::string& phase,
                                 bool reset_attempt_timer,
                                 bool activate_server) {
    state_.set_connecting(true);

    auto now = std::chrono::steady_clock::now();
    long long elapsed_ms = 0;
    {
        std::lock_guard lk(connection_trace_mu_);
        if (reset_attempt_timer || !has_connection_attempt_) {
            connection_attempt_started_ = now;
            has_connection_attempt_ = true;
        }
        elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - connection_attempt_started_).count();
    }

    std::ostringstream oss;
    oss << "[+" << elapsed_ms << "ms] " << phase;
    log_server_event(oss.str(), activate_server);
}

void App::handle_command_response(const CommandResponse& response) {
    const std::string command = ascii_lower_copy(response.command());
    std::string prefix = response.success() ? "[ok] " : "[fail] ";
    log_server_event(prefix + command + ": " + response.message(), false);

    if (command == "quota" && response.success()) {
        std::string summary = response.message();
        const auto newline = summary.find('\n');
        if (newline != std::string::npos) {
            summary = summary.substr(newline + 1);
        }
        summary = std::regex_replace(summary, std::regex(R"(\r\n?)"), "\n");
        summary = trim_ascii_whitespace(summary);
        {
            std::lock_guard lk(quota_summary_mu_);
            quota_summary_text_ = summary;
        }
        if (ui_) ui_->notify();
    }

    // Show failed commands also in the active channel for visibility
    if (!response.success()) {
        auto active = state_.active_channel().value_or("");
        if (!active.empty() && active != "server") {
            log_system_message(active, prefix + command + ": " + response.message(), false);
        }
    }

    if (command == "rmfile" && response.success()) {
        request_quota_summary();
    }

    if (command == "join") {
        std::optional<std::string> target;
        {
            std::lock_guard lk(pending_command_mu_);
            if (!pending_joins_.empty()) {
                target = pending_joins_.front();
                pending_joins_.pop_front();
            }
        }

        if (response.success() && target) {
            state_.post_ui([this, target = *target]() {
                state_.ensure_channel(target);
                state_.set_active_channel(target);
            });
            remember_target(*target);
            if (ui_) ui_->notify();
        }
        return;
    }

    if (command == "names" && response.success()) {
        // Parse "Users in #channel:\n@op\n+voiced\nregular\n"
        const auto& msg = response.message();
        auto header_end = msg.find(":\n");
        if (header_end != std::string::npos) {
            // Extract channel name from "Users in #channel"
            std::string channel = msg.substr(9, header_end - 9);

            std::vector<ChannelUserInfo> users;
            std::istringstream lines(msg.substr(header_end + 2));
            std::string line;
            while (std::getline(lines, line)) {
                if (line.empty()) continue;
                ChannelUserInfo info;
                if (line[0] == '@') {
                    info.role = UserRole::Admin;
                    info.user_id = line.substr(1);
                } else if (line[0] == '+') {
                    info.role = UserRole::Voice;
                    info.user_id = line.substr(1);
                } else {
                    info.role = UserRole::Regular;
                    info.user_id = line;
                }
                users.push_back(std::move(info));
            }

            state_.post_ui([this, channel = std::move(channel), users = std::move(users)]() {
                state_.set_channel_users(channel, users);
            });
            if (ui_) ui_->notify();
        }
        return;
    }

    if (command == "part") {
        std::optional<std::string> target;
        {
            std::lock_guard lk(pending_command_mu_);
            if (!pending_parts_.empty()) {
                target = pending_parts_.front();
                pending_parts_.pop_front();
            }
        }

        if (response.success() && target) {
            state_.post_ui([this, target = *target]() {
                state_.remove_channel(target);
            });
            forget_target(*target);
            if (ui_) ui_->notify();
        }
    }
}

void App::clear_pending_channel_commands() {
    std::lock_guard lk(pending_command_mu_);
    pending_joins_.clear();
    pending_parts_.clear();
}

bool App::has_pending_command(const std::deque<std::string>& queue,
                              const std::string& target) const {
    return std::find(queue.begin(), queue.end(), target) != queue.end();
}

void App::switch_channel(int delta) {
    auto channels = state_.channel_list();
    if (channels.empty()) return;
    auto active = state_.active_channel().value_or(channels[0]);
    auto it = std::find(channels.begin(), channels.end(), active);
    int  idx = (it == channels.end()) ? 0 : static_cast<int>(it - channels.begin());
    idx = (idx + delta + static_cast<int>(channels.size())) % static_cast<int>(channels.size());
    switch_to_channel(channels[idx]);
}

void App::switch_to_channel_by_index(int index) {
    auto channels = state_.channel_list();
    if (index < 0 || index >= static_cast<int>(channels.size())) return;
    switch_to_channel(channels[index]);
}

void App::switch_to_channel(const std::string& channel_id) {
    note_local_activity(true);
    stop_local_typing();
    auto canonical_id = canonical_channel_id(channel_id);
    const std::string switch_message = i18n::tr(i18n::I18nKey::SWITCHED_TO, canonical_id);
    const auto current_active = canonical_channel_id(state_.active_channel().value_or(""));
    if (!canonical_id.empty() && canonical_id == current_active) {
        if (ui_ && ui_->is_files_panel_visible() && !is_server_channel(canonical_id)) {
            request_remote_files_for_target(canonical_id, false);
        }
        return;
    }
    state_.ensure_channel(canonical_id);
    const auto ch_state = state_.channel_snapshot(canonical_id);
    const bool duplicate_switch_message =
        !ch_state.messages.empty() &&
        ch_state.messages.back().type == Message::Type::System &&
        ch_state.messages.back().content == switch_message;
    state_.set_active_channel(canonical_id);
    remember_target(canonical_id);
    if (!duplicate_switch_message) {
        ui_->push_system_msg(switch_message);
    }
    if (ui_ && ui_->is_files_panel_visible() && !is_server_channel(canonical_id)) {
        request_remote_files_for_target(canonical_id, false);
    }
}

void App::open_settings() {
    // Get public key for display
    std::string pubkey_hex = get_public_key_hex();
    const bool was_sharing_typing = cfg_.privacy.share_typing_indicators;
    const bool was_sharing_receipts = cfg_.privacy.share_read_receipts;
    const bool was_auto_away_enabled = cfg_.privacy.auto_away_enabled;
    const int was_auto_away_minutes = cfg_.privacy.auto_away_minutes;
    
    // Create a new screen for settings (modal)
    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
    
    ui::SettingsScreen settings_screen;
    auto result = settings_screen.show(cfg_, screen, pubkey_hex,
        [this](const std::string& theme) { on_theme_changed(theme); });
    
    switch (result) {
        case ui::SettingsResult::Saved:
            save_current_config();
            refresh_runtime_capabilities();
            if (was_sharing_typing && !cfg_.privacy.share_typing_indicators) {
                stop_local_typing();
            }
            if (!was_sharing_receipts && cfg_.privacy.share_read_receipts) {
                on_active_channel_changed(state_.active_channel().value_or(""));
            }
            if (was_auto_away_enabled && !cfg_.privacy.auto_away_enabled) {
                cancel_auto_away();
                if (auto_away_active_ && msg_handler_ && msg_handler_->is_authenticated()) {
                    msg_handler_->send_command("back", {});
                }
                auto_away_active_ = false;
            } else if (cfg_.privacy.auto_away_enabled &&
                       (!was_auto_away_enabled || was_auto_away_minutes != cfg_.privacy.auto_away_minutes)) {
                schedule_auto_away();
            }
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::SETTINGS_SAVED));
            break;
        case ui::SettingsResult::Cancelled:
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::SETTINGS_CANCELLED));
            break;
        case ui::SettingsResult::Logout:
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::LOGGING_OUT));
            should_exit_ = true;
            // Trigger exit
            ioc_.stop();
            break;
    }
    ui_->notify();
}

std::string App::build_transfer_summary() const {
    if (!file_mgr_) {
        return {};
    }

    const auto active = file_mgr_->get_active_transfers();
    if (active.empty()) {
        return {};
    }

    std::string summary = "Transfers ";
    const std::size_t visible = std::min<std::size_t>(2, active.size());
    for (std::size_t i = 0; i < visible; ++i) {
        if (i > 0) {
            summary += " | ";
        }
        const auto& info = active[i];
        summary += (info.direction == client::file::TransferDirection::UPLOAD ? "U " : "D ");
        summary += info.filename.empty() ? info.file_id : info.filename;
        summary += " ";
        summary += transfer_progress_label(info);
    }
    if (active.size() > visible) {
        summary += " +" + std::to_string(active.size() - visible) + " more";
    }
    return summary;
}

std::vector<std::string> App::format_transfer_lines(std::size_t limit) const {
    std::vector<std::string> lines;
    if (!file_mgr_) {
        return lines;
    }

    auto transfers = file_mgr_->list_transfers(limit);
    if (transfers.empty()) {
        return lines;
    }

    lines.push_back("Recent transfers:");
    for (const auto& info : transfers) {
        std::string line = "  [" + transfer_direction_label(info.direction) + "] " +
                           info.transfer_id + " " +
                           transfer_state_label(info.state) + " " +
                           transfer_progress_label(info) + " " +
                           (info.filename.empty() ? info.file_id : info.filename);

        if (info.direction == client::file::TransferDirection::UPLOAD) {
            line += " -> " + transfer_target_label(info);
        } else if (!info.local_path.empty()) {
            line += " -> " + info.local_path.string();
        }

        if (!info.error_message.empty()) {
            line += " | " + info.error_message;
        }
        lines.push_back(std::move(line));
    }

    return lines;
}

void App::update_file_transfer_policy(const FileTransferPolicy& policy) {
    file_transfer_policy_.received = true;
    file_transfer_policy_.max_upload_bytes = policy.max_upload_bytes();
    file_transfer_policy_.max_total_storage_bytes = policy.max_total_storage_bytes();
    file_transfer_policy_.max_user_storage_bytes = policy.max_user_storage_bytes();
    file_transfer_policy_.allowed_mime_types.clear();
    file_transfer_policy_.blocked_mime_types.clear();

    file_transfer_policy_.allowed_mime_types.reserve(
        static_cast<std::size_t>(policy.allowed_mime_types_size()));
    for (const auto& mime : policy.allowed_mime_types()) {
        file_transfer_policy_.allowed_mime_types.push_back(ascii_lower_copy(mime));
    }

    file_transfer_policy_.blocked_mime_types.reserve(
        static_cast<std::size_t>(policy.blocked_mime_types_size()));
    for (const auto& mime : policy.blocked_mime_types()) {
        file_transfer_policy_.blocked_mime_types.push_back(ascii_lower_copy(mime));
    }

    spdlog::info("Received file transfer policy: max_upload_bytes={}, max_total_storage_bytes={}, max_user_storage_bytes={}, allowed_mimes={}, blocked_mimes={}",
                 file_transfer_policy_.max_upload_bytes,
                 file_transfer_policy_.max_total_storage_bytes,
                 file_transfer_policy_.max_user_storage_bytes,
                 file_transfer_policy_.allowed_mime_types.size(),
                 file_transfer_policy_.blocked_mime_types.size());
}

void App::handle_file_list_response(const FileListResponse& response) {
    const std::string target = canonical_file_target(response.recipient_id(), response.channel_id());
    if (target.empty()) {
        return;
    }

    std::vector<RemoteFileEntry> files;
    files.reserve(static_cast<std::size_t>(response.files_size()));
    for (const auto& file : response.files()) {
        RemoteFileEntry entry;
        entry.file_id = file.file_id();
        entry.filename = file.filename();
        entry.file_size = file.file_size();
        entry.mime_type = file.mime_type();
        entry.sender_id = file.sender_id();
        entry.recipient_id = file.recipient_id();
        entry.channel_id = file.channel_id();
        entry.uploaded_at = file.uploaded_at();
        entry.expires_at = file.expires_at();
        files.push_back(std::move(entry));
    }

    std::sort(files.begin(), files.end(), [](const RemoteFileEntry& a, const RemoteFileEntry& b) {
        if (a.uploaded_at != b.uploaded_at) {
            return a.uploaded_at > b.uploaded_at;
        }
        if (a.filename != b.filename) {
            return a.filename < b.filename;
        }
        return a.file_id < b.file_id;
    });

    state_.post_ui([this, target, files]() {
        state_.set_channel_files(target, files);
    });

    bool echo = false;
    {
        std::lock_guard lk(remote_file_mu_);
        echo = pending_file_list_echo_targets_.erase(target) > 0;
    }

    if (echo && ui_) {
        if (files.empty()) {
            ui_->push_system_msg("No files available for " + target + ".");
        } else {
            ui_->push_system_msg("Files for " + target + ":");
            const std::size_t count = std::min<std::size_t>(files.size(), 12);
            for (std::size_t i = 0; i < count; ++i) {
                const auto& file = files[i];
                ui_->push_system_msg(
                    "  " + file.filename + " | " + human_bytes(file.file_size) +
                    " | " + file.file_id);
            }
            if (files.size() > count) {
                ui_->push_system_msg("  ... +" + std::to_string(files.size() - count) + " more (F3 panel)");
            }
        }
        ui_->notify();
    } else if (ui_) {
        ui_->notify();
    }
}

void App::handle_file_changed(const FileChanged& changed) {
    const std::string target = canonical_file_target(changed.recipient_id(), changed.channel_id());
    if (target.empty()) {
        return;
    }

    request_quota_summary();

    const std::string active = canonical_channel_id(state_.active_channel().value_or(""));
    if (active != target) {
        return;
    }

    if (is_server_channel(target)) {
        return;
    }

    request_remote_files_for_target(target, false);
}

void App::handle_typing_update(const TypingUpdate& typing) {
    if (typing.sender_id().empty() || typing.sender_id() == cfg_.identity.user_id) {
        return;
    }

    std::string target = typing.target_id();
    if (target.empty()) {
        return;
    }
    if (target.front() != '#') {
        target = typing.sender_id();
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard lk(typing_mu_);
        if (typing.is_typing()) {
            remote_typing_[target][typing.sender_id()] = now + kTypingRemoteTtl;
        } else {
            auto target_it = remote_typing_.find(target);
            if (target_it != remote_typing_.end()) {
                target_it->second.erase(typing.sender_id());
                if (target_it->second.empty()) {
                    remote_typing_.erase(target_it);
                }
            }
        }
        prune_remote_typing_locked(now);
    }

    schedule_remote_typing_cleanup();
    if (ui_) {
        ui_->notify();
    }
}

void App::handle_read_receipt(const ReadReceipt& receipt) {
    if (receipt.reader_id().empty() ||
        receipt.target_id() != cfg_.identity.user_id ||
        receipt.message_id().empty()) {
        return;
    }

    const std::string channel_id = receipt.reader_id();
    state_.post_ui([this, channel_id, message_id = receipt.message_id(), read_at_ms = receipt.read_at_ms()]() {
        if (state_.mark_direct_messages_read_by_remote(
                channel_id, cfg_.identity.user_id, message_id, read_at_ms)) {
            if (ui_) {
                ui_->notify();
            }
        }
    });
}

void App::note_local_activity(bool trigger_auto_back) {
    last_local_activity_ = std::chrono::steady_clock::now();

    if (auto_away_active_ &&
        trigger_auto_back &&
        msg_handler_ &&
        msg_handler_->is_authenticated() &&
        net_client_ &&
        net_client_->is_connected()) {
        msg_handler_->send_command("back", {});
        auto_away_active_ = false;
    }

    schedule_auto_away();
}

void App::schedule_auto_away() {
    auto_away_timer_.cancel();
    if (!cfg_.privacy.auto_away_enabled) {
        return;
    }

    const auto timeout = auto_away_timeout(cfg_);
    auto_away_timer_.expires_after(timeout);
    auto_away_timer_.async_wait([this, timeout](const boost::system::error_code& ec) {
        if (ec || !cfg_.privacy.auto_away_enabled) {
            return;
        }

        if (std::chrono::steady_clock::now() - last_local_activity_ < timeout) {
            schedule_auto_away();
            return;
        }

        if (!msg_handler_ || !msg_handler_->is_authenticated() || !net_client_ || !net_client_->is_connected()) {
            schedule_auto_away();
            return;
        }

        const auto local_presence = state_.presence(cfg_.identity.user_id);
        if (local_presence != PresenceStatus::Online) {
            if (!auto_away_active_) {
                schedule_auto_away();
            }
            return;
        }

        msg_handler_->send_command("away", {});
        auto_away_active_ = true;
    });
}

void App::cancel_auto_away() {
    auto_away_timer_.cancel();
}

void App::track_dm_read_candidate(const std::string& channel_id, const Message& msg) {
    if (!is_direct_target(channel_id) || msg.message_id.empty()) {
        return;
    }

    const std::string active = canonical_channel_id(state_.active_channel().value_or(""));
    if (cfg_.privacy.share_read_receipts && active == channel_id) {
        send_read_receipt(channel_id, msg.message_id);
        return;
    }

    std::lock_guard lk(pending_read_receipts_mu_);
    pending_read_receipts_[channel_id] = msg.message_id;
}

void App::on_active_channel_changed(const std::string& channel_id) {
    stop_local_typing();
    flush_pending_read_receipt_for_channel(canonical_channel_id(channel_id));
}

void App::send_read_receipt(const std::string& target, const std::string& message_id) {
    if (!cfg_.privacy.share_read_receipts ||
        target.empty() ||
        !is_direct_target(target) ||
        message_id.empty() ||
        !net_client_ ||
        !msg_handler_ ||
        !msg_handler_->is_authenticated() ||
        !net_client_->is_connected()) {
        return;
    }

    ReadReceipt receipt;
    receipt.set_reader_id(cfg_.identity.user_id);
    receipt.set_target_id(target);
    receipt.set_message_id(message_id);
    receipt.set_read_at_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    Envelope env;
    env.set_type(MT_READ_RECEIPT);
    env.set_payload(receipt.SerializeAsString());
    env.set_timestamp_ms(receipt.read_at_ms());
    net_client_->send(env);
}

void App::flush_pending_read_receipt_for_channel(const std::string& channel_id) {
    if (!cfg_.privacy.share_read_receipts || !is_direct_target(channel_id)) {
        return;
    }

    std::string message_id;
    {
        std::lock_guard lk(pending_read_receipts_mu_);
        auto it = pending_read_receipts_.find(channel_id);
        if (it == pending_read_receipts_.end()) {
            return;
        }
        message_id = std::move(it->second);
        pending_read_receipts_.erase(it);
    }

    send_read_receipt(channel_id, message_id);
}

void App::request_remote_files_for_target(const std::string& target, bool echo_to_chat) {
    if (!msg_handler_ || !msg_handler_->is_authenticated()) {
        if (echo_to_chat && ui_) {
            ui_->push_system_msg(i18n::tr(i18n::I18nKey::NOT_CONNECTED_CHECK_STATUS));
        }
        return;
    }

    const auto canonical_target = canonical_channel_id(target);
    if (canonical_target.empty() || is_server_channel(canonical_target)) {
        if (echo_to_chat && ui_) {
            ui_->push_system_msg("Open a channel or DM before listing files.");
        }
        return;
    }

    if (echo_to_chat) {
        std::lock_guard lk(remote_file_mu_);
        pending_file_list_echo_targets_.insert(canonical_target);
    }

    const std::string recipient = is_direct_target(canonical_target) ? canonical_target : "";
    const std::string channel = is_channel_target(canonical_target) ? canonical_target : "";
    msg_handler_->request_file_list(recipient, channel, 100);
}

void App::download_remote_file(const RemoteFileEntry& file) {
    if (!file_mgr_ || !ui_) {
        return;
    }

    std::filesystem::path save_path = default_download_path_for_file(file.file_id, file.filename);
    const auto parent = save_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    auto tid = file_mgr_->download(file.file_id, save_path);
    if (tid.empty()) {
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::DOWNLOAD_FAILED));
    } else {
        const std::string label = file.filename.empty() ? file.file_id : file.filename;
        ui_->push_system_msg(i18n::tr(i18n::I18nKey::DOWNLOADING,
                                      label,
                                      save_path.string()));
        ui_->push_system_msg(
            "Download queued: " + label + " -> " + save_path.string() +
            " (transfer " + tid + ", see /transfers)");
    }
    ui_->notify();
}

void App::delete_remote_file(const RemoteFileEntry& file) {
    if (!msg_handler_ || !msg_handler_->is_authenticated() || !ui_) {
        return;
    }

    const std::string label = file.filename.empty() ? file.file_id : file.filename;
    msg_handler_->send_command("rmfile", {file.file_id});
    ui_->push_system_msg("Delete requested: " + label + " (" + file.file_id + ")");
    ui_->notify();
}

void App::request_quota_summary() {
    if (!msg_handler_ || !msg_handler_->is_authenticated()) {
        return;
    }
    msg_handler_->send_command("quota", {});
}

std::string App::files_panel_quota_summary() const {
    std::lock_guard lk(quota_summary_mu_);
    return quota_summary_text_;
}

std::string App::build_typing_summary() const {
    const std::string active = canonical_channel_id(state_.active_channel().value_or(""));
    if (active.empty() || is_server_channel(active)) {
        return {};
    }

    std::vector<std::string> users;
    {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lk(typing_mu_);
        prune_remote_typing_locked(now);
        auto it = remote_typing_.find(active);
        if (it != remote_typing_.end()) {
            users.reserve(it->second.size());
            for (const auto& [user_id, _] : it->second) {
                users.push_back(user_id);
            }
        }
    }

    if (users.empty()) {
        return {};
    }

    std::sort(users.begin(), users.end());
    if (users.size() == 1) {
        return users[0] + " is typing...";
    }
    if (users.size() == 2) {
        return users[0] + " and " + users[1] + " are typing...";
    }
    return users[0] + ", " + users[1] + " +" + std::to_string(users.size() - 2) + " typing...";
}

void App::stop_local_typing() {
    typing_idle_timer_.cancel();
    if (!local_typing_active_ || local_typing_target_.empty()) {
        return;
    }
    send_typing_update(local_typing_target_, false);
    local_typing_active_ = false;
    local_typing_target_.clear();
}

void App::send_typing_update(const std::string& target, bool is_typing) {
    if (!net_client_ || !msg_handler_ || !msg_handler_->is_authenticated() || !net_client_->is_connected()) {
        return;
    }
    if (is_typing && !cfg_.privacy.share_typing_indicators) {
        return;
    }

    TypingUpdate update;
    update.set_sender_id(cfg_.identity.user_id);
    update.set_target_id(target);
    update.set_is_typing(is_typing);

    Envelope env;
    env.set_type(MT_TYPING);
    env.set_payload(update.SerializeAsString());
    env.set_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    net_client_->send(env);
}

void App::schedule_remote_typing_cleanup() {
    std::optional<std::chrono::steady_clock::time_point> next_expiry;
    {
        std::lock_guard lk(typing_mu_);
        const auto now = std::chrono::steady_clock::now();
        prune_remote_typing_locked(now);
        for (const auto& [_, users] : remote_typing_) {
            for (const auto& [_, expiry] : users) {
                if (!next_expiry || expiry < *next_expiry) {
                    next_expiry = expiry;
                }
            }
        }
    }

    typing_cleanup_timer_.cancel();
    if (!next_expiry) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto delay = (*next_expiry > now)
        ? (*next_expiry - now)
        : std::chrono::steady_clock::duration::zero();
    typing_cleanup_timer_.expires_after(delay);
    typing_cleanup_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }
        {
            std::lock_guard lk(typing_mu_);
            prune_remote_typing_locked(std::chrono::steady_clock::now());
        }
        schedule_remote_typing_cleanup();
        if (ui_) {
            ui_->notify();
        }
    });
}

void App::prune_remote_typing_locked(std::chrono::steady_clock::time_point now) const {
    for (auto target_it = remote_typing_.begin(); target_it != remote_typing_.end();) {
        auto& users = target_it->second;
        for (auto user_it = users.begin(); user_it != users.end();) {
            if (user_it->second <= now) {
                user_it = users.erase(user_it);
            } else {
                ++user_it;
            }
        }
        if (users.empty()) {
            target_it = remote_typing_.erase(target_it);
        } else {
            ++target_it;
        }
    }
}

bool App::handle_file_transfer_error(const FileError& error) {
    if (!is_quota_error_message(error.error_message())) {
        return false;
    }

    request_quota_summary();

    if (!ui_ || !file_mgr_) {
        return true;
    }

    const auto info = file_mgr_->get_transfer_by_file_id(error.file_id());
    const std::string label = (info && !info->filename.empty()) ? info->filename : error.file_id();

    std::string message = "Upload failed: " + label + " ";
    if (error.error_message() == "User storage quota exceeded") {
        message += "hit your storage quota. See /quota.";
    } else {
        message += "hit the server storage quota. See /quota.";
    }

    if (info && info->direction == client::file::TransferDirection::UPLOAD) {
        const std::string target = !info->channel_id.empty() ? info->channel_id : info->recipient_id;
        if (!target.empty() && !is_server_channel(target)) {
            log_system_message(target, message, false);
            if (ui_) {
                ui_->notify();
            }
            return true;
        }
    }

    ui_->push_system_msg(message);
    ui_->notify();
    return true;
}

std::string App::get_public_key_hex() const {
    if (!crypto_ || !store_) return "";
    
    // Get the identity public key from crypto engine
    // This is a simplified version - in production, get from crypto_->identity()
    // For now return empty string - would need proper integration with crypto engine
    return "(public key display not yet implemented)";
}

void App::on_theme_changed(const std::string& theme_name) {
    cfg_.ui.theme = theme_name;
    ui::apply_theme(theme_name);
    // Note: In a full implementation, this would update the color scheme
    // and refresh the UI immediately
}

void App::save_current_config() {
    save_config(cfg_, config_path_);
}

void App::refresh_runtime_capabilities() {
    runtime_capabilities_ = detect_runtime_capabilities(cfg_);
    spdlog::info(
        "Runtime capabilities: terminal_mode={}, detected_protocol={}, compositor_protocol={}, inline_native={}, clipboard_backend={}, audio_in={}, audio_out={}",
        terminal_graphics_mode_name(runtime_capabilities_.configured_terminal_graphics_mode),
        terminal_inline_protocol_name(runtime_capabilities_.terminal_protocol_detected),
        terminal_inline_protocol_name(runtime_capabilities_.compositor_protocol),
        runtime_capabilities_.inline_native_enabled,
        runtime_capabilities_.clipboard_backend,
        runtime_capabilities_.audio_input_device_count,
        runtime_capabilities_.audio_output_device_count);
}

} // namespace grotto
