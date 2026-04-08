#pragma once

#include "session.hpp"
#include "rate_limiter.hpp"
#include "db/user_store.hpp"
#include "db/offline_store.hpp"
#include "db/file_store.hpp"
#include "commands/command_handler.hpp"
#include "voice/voice_room_manager.hpp"
#include "utils/nickname_utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl/context.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

namespace grotto::net {

// Main server class that accepts connections and manages sessions
class Listener : public ServerContext {
public:
    Listener(
        boost::asio::io_context& ioc,
        boost::asio::ssl::context& ssl_ctx,
        const std::string& host,
        uint16_t port,
        db::UserStore& user_store,
        db::OfflineStore& offline_store);

    ~Listener() override;

    // Start accepting connections
    void run();

    // Initiate graceful shutdown
    void shutdown();

    // ServerContext implementation
    void on_session_authenticated(std::shared_ptr<Session> session) override;
    void on_session_disconnected(std::shared_ptr<Session> session, const std::string& reason) override;
    void broadcast(const Envelope& env, std::shared_ptr<Session> exclude = nullptr) override;
    void broadcast_presence(const PresenceUpdate& update,
                            std::shared_ptr<Session> exclude = nullptr) override;
    std::shared_ptr<Session> find_session(const std::string& user_id) override;
    std::shared_ptr<Session> find_session_by_nickname(const std::string& nickname) override;
    bool is_nickname_available(const std::string& nickname, 
                               const std::string& exclude_user_id = "") override;
    db::UserStore& user_store() override { return user_store_; }
    db::OfflineStore& offline_store() override { return offline_store_; }
    db::Database& database() override;
    db::FileStore& file_store() override { return *file_store_; }
    commands::CommandHandler* command_handler() override { return command_handler_.get(); }
    voice::VoiceRoomManager& voice_room_manager() override { return *voice_room_mgr_; }
    
    // Set file store reference
    void set_file_store(db::FileStore& file_store) { file_store_ = &file_store; }

    // Getters
    int ping_interval_sec() const override { return ping_interval_sec_; }
    int ping_timeout_sec() const override { return ping_timeout_sec_; }
    int msg_rate_per_sec() const override { return msg_rate_per_sec_; }
    size_t max_chat_payload_bytes() const override { return max_chat_payload_bytes_; }
    uint64_t max_upload_bytes() const override { return max_upload_bytes_; }
    uint64_t max_total_storage_bytes() const override { return max_total_storage_bytes_; }
    uint64_t max_user_storage_bytes() const override { return max_user_storage_bytes_; }
    const std::string& motd() const override { return motd_; }
    const std::vector<std::string>& allowed_mime_types() const override { return allowed_mime_types_; }
    const std::vector<std::string>& blocked_mime_types() const override { return blocked_mime_types_; }
    const std::vector<std::string>& voice_ice_servers() const override { return voice_ice_servers_; }
    const std::string& voice_turn_username() const override { return voice_turn_username_; }
    const std::string& voice_turn_password() const override { return voice_turn_password_; }

    // Set limits from config
    void set_ping_intervals(int interval_sec, int timeout_sec);
    void set_rate_limits(int msg_rate_per_sec, int conn_rate_per_min);
    void set_max_chat_payload_bytes(size_t max_chat_payload_bytes) { max_chat_payload_bytes_ = max_chat_payload_bytes; }
    void set_file_policy(uint64_t max_upload_bytes,
                         uint64_t max_total_storage_bytes,
                         uint64_t max_user_storage_bytes,
                         std::vector<std::string> allowed_mime_types,
                         std::vector<std::string> blocked_mime_types) {
        max_upload_bytes_ = max_upload_bytes;
        max_total_storage_bytes_ = max_total_storage_bytes;
        max_user_storage_bytes_ = max_user_storage_bytes;
        allowed_mime_types_ = std::move(allowed_mime_types);
        blocked_mime_types_ = std::move(blocked_mime_types);
    }
    void set_max_connections(int max_connections);
    void set_motd(const std::string& motd) { motd_ = motd; }
    void set_voice_ice_config(const std::vector<std::string>& ice_servers,
                              const std::string& turn_username,
                              const std::string& turn_password) {
        voice_ice_servers_ = ice_servers;
        voice_turn_username_ = turn_username;
        voice_turn_password_ = turn_password;
    }

    // Set database reference (for command handler)
    void set_database(db::Database& db) { db_ = &db; }

    // Get list of online users (user_id, remote_endpoint)
    std::vector<std::pair<std::string, std::string>> get_online_users() const;

    // Get current active connection count
    int active_connection_count() const { return active_connections_.load(); }

private:
    void do_accept();
    void on_signal(boost::system::error_code ec, int signal_number);
    void cleanup_dead_sessions();
    void send_presence_snapshot(const std::shared_ptr<Session>& session,
                                const std::vector<std::string>& online_user_ids);

    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using Acceptor = boost::asio::ip::tcp::acceptor;
    using Socket = boost::asio::ip::tcp::socket;
    using SignalSet = boost::asio::signal_set;

    Strand strand_;
    Acceptor acceptor_;
    Socket socket_;
    boost::asio::ssl::context& ssl_ctx_;
    SignalSet signals_;

    // DB stores
    db::UserStore& user_store_;
    db::OfflineStore& offline_store_;
    db::Database* db_ = nullptr;
    db::FileStore* file_store_ = nullptr;

    // Command handler
    std::unique_ptr<commands::CommandHandler> command_handler_;

    // Voice room manager
    std::unique_ptr<voice::VoiceRoomManager> voice_room_mgr_;

    // Active sessions
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_by_user_;
    std::unordered_map<std::shared_ptr<Session>, std::string> users_by_session_;
    mutable std::mutex sessions_mutex_;

    // Config limits
    int ping_interval_sec_ = 30;
    int ping_timeout_sec_ = 60;
    int msg_rate_per_sec_ = 20;
    int conn_rate_per_min_ = 10;
    int max_connections_ = 100;
    size_t max_chat_payload_bytes_ = 8192;
    uint64_t max_upload_bytes_ = 100ull * 1024ull * 1024ull;
    uint64_t max_total_storage_bytes_ = 0;
    uint64_t max_user_storage_bytes_ = 0;
    std::vector<std::string> allowed_mime_types_;
    std::vector<std::string> blocked_mime_types_;

    // Total active connections (pre-auth + authenticated)
    std::atomic<int> active_connections_{0};

    // Per-IP connection rate limiters
    std::unordered_map<std::string, RateLimiter> ip_rate_map_;
    std::mutex ip_rate_mutex_;

    // Shutdown flag
    std::atomic<bool> shutting_down_{false};
    
    // MOTD (Message of the Day)
    std::string motd_;
    std::vector<std::string> voice_ice_servers_;
    std::string voice_turn_username_;
    std::string voice_turn_password_;
};

} // namespace grotto::net
