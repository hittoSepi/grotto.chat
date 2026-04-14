#pragma once

#include "config.hpp"
#include "state/app_state.hpp"
#include "ui/ui_manager.hpp"
#include "net/net_client.hpp"
#include "net/message_handler.hpp"
#include "crypto/crypto_engine.hpp"
#include "voice/voice_engine.hpp"
#include "preview/link_previewer.hpp"
#include "db/local_store.hpp"
#include "input/command_parser.hpp"
#include "file/file_transfer.hpp"
#include "help/help_manager.hpp"
#include "runtime/runtime_capabilities.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace grotto {

class App {
public:
    App();
    ~App();

    bool init(const std::filesystem::path& config_path,
              const std::string& user_id_override = {},
              const std::optional<std::pair<std::string, uint16_t>>& server_url = std::nullopt);

    // Runs FTXUI event loop on main thread (blocks until quit).
    int run();

private:
    // Called by UIManager when user presses Enter
    void on_submit(const std::string& line);
    void on_input_changed(const std::string& text);
    void handle_command(const ParsedCommand& cmd);
    void send_chat(const std::string& text);
    void switch_channel(int delta);
    void switch_to_channel(const std::string& channel_id);
    // Called by UIManager on Alt+1..9; index is 0-based into sorted channel list.
    void switch_to_channel_by_index(int index);
    void persist_message(const std::string& channel_id, const Message& msg);
    void trigger_previews(const std::string& channel_id, const std::string& text);
    void log_system_message(const std::string& channel_id,
                            const std::string& text,
                            bool activate_channel = false);
    void log_server_event(const std::string& text, bool activate_server = false);
    void trace_connection_phase(const std::string& phase,
                                bool reset_attempt_timer = false,
                                bool activate_server = false);
    void handle_command_response(const CommandResponse& response);
    bool handle_file_transfer_error(const FileError& error);
    void clear_pending_channel_commands();
    bool has_pending_command(const std::deque<std::string>& queue,
                             const std::string& target) const;
    void restore_remembered_targets();
    void remember_target(const std::string& channel_id);
    void forget_target(const std::string& channel_id);

    // Open settings screen
    void open_settings();
    
    // Get public key hex for display in settings
    std::string get_public_key_hex() const;
    
    // Apply theme change immediately
    void on_theme_changed(const std::string& theme_name);
    
    // Save config to file
    void save_current_config();
    void refresh_runtime_capabilities();
    std::string build_transfer_summary() const;
    std::string build_connection_summary() const;
    std::vector<std::string> format_transfer_lines(std::size_t limit) const;
    void update_file_transfer_policy(const FileTransferPolicy& policy);
    void handle_file_list_response(const FileListResponse& response);
    void handle_file_changed(const FileChanged& changed);
    void request_remote_files_for_target(const std::string& target, bool echo_to_chat = false);
    void download_remote_file(const RemoteFileEntry& file);
    void delete_remote_file(const RemoteFileEntry& file);
    void request_quota_summary();
    std::string files_panel_quota_summary() const;
    void handle_typing_update(const TypingUpdate& typing);
    void handle_read_receipt(const ReadReceipt& receipt);
    void note_local_activity(bool trigger_auto_back);
    void schedule_auto_away();
    void cancel_auto_away();
    void track_dm_read_candidate(const std::string& channel_id, const Message& msg);
    void on_active_channel_changed(const std::string& channel_id);
    std::string build_typing_summary() const;
    void stop_local_typing();
    void send_typing_update(const std::string& target, bool is_typing);
    void send_read_receipt(const std::string& target, const std::string& message_id);
    void flush_pending_read_receipt_for_channel(const std::string& channel_id);
    void schedule_remote_typing_cleanup();
    void prune_remote_typing_locked(std::chrono::steady_clock::time_point now) const;
    void begin_shutdown(bool request_ui_exit = true, bool persist_config = true);
    void finish_shutdown();
    void shutdown_voice_session();

    struct FileTransferPolicyState {
        bool received = false;
        uint64_t max_upload_bytes = 0;
        uint64_t max_total_storage_bytes = 0;
        uint64_t max_user_storage_bytes = 0;
        std::vector<std::string> allowed_mime_types;
        std::vector<std::string> blocked_mime_types;
    };

    ClientConfig cfg_;
    RuntimeCapabilities runtime_capabilities_;
    std::filesystem::path config_path_;
    std::string server_version_ = "unknown (server does not advertise version yet)";

    std::unique_ptr<db::LocalStore>         store_;
    AppState                                state_;
    std::unique_ptr<ui::UIManager>          ui_;
    std::unique_ptr<crypto::CryptoEngine>   crypto_;
    std::unique_ptr<voice::VoiceEngine>     voice_;
    std::unique_ptr<LinkPreviewer>          previewer_;
    std::unique_ptr<HelpManager>            help_;
    std::unique_ptr<client::file::FileTransferManager> file_mgr_;

    boost::asio::io_context                 ioc_;
    boost::asio::steady_timer               typing_idle_timer_{ioc_};
    boost::asio::steady_timer               typing_cleanup_timer_{ioc_};
    boost::asio::steady_timer               auto_away_timer_{ioc_};
    std::shared_ptr<net::NetClient>         net_client_;
    std::unique_ptr<net::MessageHandler>    msg_handler_;

    std::thread io_thread_;

    mutable std::mutex pending_command_mu_;
    std::deque<std::string> pending_joins_;
    std::deque<std::string> pending_parts_;

    mutable std::mutex connection_trace_mu_;
    std::chrono::steady_clock::time_point connection_attempt_started_{};
    bool has_connection_attempt_ = false;
    std::string connection_status_summary_;
    std::string last_disconnect_reason_;
    FileTransferPolicyState file_transfer_policy_;
    mutable std::mutex remote_file_mu_;
    std::unordered_set<std::string> pending_file_list_echo_targets_;
    mutable std::mutex quota_summary_mu_;
    std::string quota_summary_text_;
    mutable std::mutex typing_mu_;
    mutable std::unordered_map<std::string, std::unordered_map<std::string, std::chrono::steady_clock::time_point>> remote_typing_;
    mutable std::mutex pending_read_receipts_mu_;
    std::unordered_map<std::string, std::string> pending_read_receipts_;
    std::unordered_map<std::string, std::string> dm_presence_notice_keys_;
    std::string local_typing_target_;
    bool local_typing_active_ = false;
    bool auto_away_active_ = false;
    std::chrono::steady_clock::time_point last_typing_sent_{};
    std::chrono::steady_clock::time_point last_local_activity_{};

    // Flag to indicate if app should exit (for settings logout)
    bool should_exit_ = false;
    std::atomic_bool shutdown_started_{false};
    bool config_saved_ = false;
};

} // namespace grotto
