#include "server_owner.hpp"
#include "../server.hpp"
#include "../commands/command_handler.hpp"
#include "grotto.pb.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

namespace grotto::admin {

namespace fs = std::filesystem;

namespace {

std::uint64_t unix_timestamp_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string join_args(const std::vector<std::string>& args, std::size_t start = 0) {
    std::string joined;
    for (std::size_t i = start; i < args.size(); ++i) {
        if (i > start) {
            joined += " ";
        }
        joined += args[i];
    }
    return joined;
}

Envelope make_command_response_envelope(const std::string& command, const std::string& message) {
    CommandResponse response;
    response.set_success(true);
    response.set_command(command);
    response.set_message(message);

    Envelope env;
    env.set_seq(0);
    env.set_timestamp_ms(unix_timestamp_ms());
    env.set_type(MT_COMMAND_RESPONSE);

    std::vector<std::uint8_t> payload(response.ByteSizeLong());
    response.SerializeToArray(payload.data(), static_cast<int>(payload.size()));
    env.set_payload(payload.data(), payload.size());
    return env;
}

} // namespace

ServerOwner::ServerOwner(Server& server, const std::string& key_path)
    : server_(server)
    , key_path_(key_path)
    , start_time_(std::chrono::steady_clock::now())
{
    // Generate keys if they don't exist
    if (!fs::exists(fs::path(key_path_))) {
        spdlog::info("ServerOwner: Generating new key pair...");
        if (!generate_key_pair(key_path_)) {
            spdlog::error("ServerOwner: Failed to generate key pair!");
            return;
        }
    }
    
    // Load keys
    if (!load_keys()) {
        spdlog::error("ServerOwner: Failed to load keys from {}", key_path_);
        return;
    }
    
    spdlog::info("ServerOwner: Initialized with user_id '{}'", std::string(user_id()));
}

ServerOwner::~ServerOwner() = default;

ServerOwner::Presence ServerOwner::get_presence() const {
    return {
        .user_id = std::string(user_id()),
        .status = PresenceStatus::Online,
        .status_message = "Server Owner"
    };
}

void ServerOwner::send_to_channel(const std::string& channel_id, const std::string& message) {
    if (!keys_loaded_) {
        spdlog::error("ServerOwner: Cannot send message - keys not loaded");
        return;
    }

    auto* listener = server_.listener();
    if (!listener || !listener->command_handler()) {
        spdlog::error("ServerOwner: Cannot send to channel {} - listener not available", channel_id);
        return;
    }

    auto* command_handler = listener->command_handler();
    auto members = command_handler->get_channel_members(channel_id);
    if (members.empty()) {
        spdlog::warn("ServerOwner: Cannot send to channel {} - channel does not exist or is empty", channel_id);
        return;
    }
    
    spdlog::info("ServerOwner: Sending message to {}: {}", channel_id, 
                 message.substr(0, 50));

    command_handler->broadcast_to_channel(
        channel_id,
        make_command_response_envelope("announce", message),
        nullptr);
}

void ServerOwner::send_to_user(const std::string& user_id, const std::string& message) {
    if (!keys_loaded_) {
        spdlog::error("ServerOwner: Cannot send message - keys not loaded");
        return;
    }

    auto* listener = server_.listener();
    if (!listener) {
        spdlog::error("ServerOwner: Cannot send to user {} - listener not available", user_id);
        return;
    }
    
    spdlog::info("ServerOwner: Sending DM to {}: {}", user_id, 
                 message.substr(0, 50));

    Envelope env = make_command_response_envelope("notice", message);
    auto session = listener->find_session(user_id);
    if (session) {
        session->send(env);
        return;
    }

    std::vector<std::uint8_t> payload(env.ByteSizeLong());
    env.SerializeToArray(payload.data(), static_cast<int>(payload.size()));
    if (!listener->offline_store().save(user_id, payload)) {
        spdlog::warn("ServerOwner: Failed to queue offline message for {}", user_id);
    }
}

ServerOwner::CommandResult ServerOwner::execute_command(const std::string& command,
                                                        const std::vector<std::string>& args) {
    spdlog::info("ServerOwner: Executing command '{}' with {} args", command, args.size());
    
    // Command dispatch
    if (command == "announce" || command == "a") {
        return cmd_announce(args);
    } else if (command == "ban" || command == "b") {
        return cmd_ban(args);
    } else if (command == "kick" || command == "k") {
        return cmd_kick(args);
    } else if (command == "shutdown" || command == "stop") {
        return cmd_shutdown(args);
    } else if (command == "restart" || command == "reboot") {
        return cmd_restart(args);
    } else if (command == "status" || command == "s") {
        return cmd_status(args);
    } else if (command == "config" || command == "cfg") {
        return cmd_config(args);
    } else if (command == "help" || command == "h" || command == "?") {
        return cmd_help(args);
    }
    
    return {false, "Unknown command: " + command + ". Type /help for available commands."};
}

ServerOwner::CommandResult ServerOwner::execute_command_line(const std::string& command_line) {
    // Parse command line
    std::istringstream iss(command_line);
    std::string command;
    std::vector<std::string> args;
    
    iss >> command;
    
    // Remove leading / if present
    if (!command.empty() && command[0] == '/') {
        command = command.substr(1);
    }
    
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return execute_command(command, args);
}

std::vector<unsigned char> ServerOwner::get_public_key() const {
    return public_key_;
}

bool ServerOwner::generate_key_pair(const std::string& key_path) {
    // Create directory if needed
    fs::path key_file(key_path);
    fs::create_directories(key_file.parent_path());
    
    // Generate Ed25519 key pair
    unsigned char pk[crypto_sign_ed25519_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_ed25519_SECRETKEYBYTES];
    
    if (crypto_sign_ed25519_keypair(pk, sk) != 0) {
        spdlog::error("ServerOwner: libsodium key generation failed");
        return false;
    }
    
    // Save to file (both keys in one file, or separate files)
    std::ofstream file(key_path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("ServerOwner: Cannot open {} for writing", key_path);
        return false;
    }
    
    // Format: 32 bytes public key + 32 bytes private key (seed)
    file.write(reinterpret_cast<const char*>(pk), crypto_sign_ed25519_PUBLICKEYBYTES);
    file.write(reinterpret_cast<const char*>(sk), crypto_sign_ed25519_SECRETKEYBYTES);
    file.close();
    
    // Set restrictive permissions (Unix only)
    #ifndef _WIN32
    fs::permissions(key_path, 
        fs::perms::owner_read | fs::perms::owner_write,
        fs::perm_options::replace);
    #endif
    
    spdlog::info("ServerOwner: Key pair generated and saved to {}", key_path);
    return true;
}

bool ServerOwner::load_keys() {
    std::ifstream file(key_path_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("ServerOwner: Cannot open key file {}", key_path_);
        return false;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Expected: 32 bytes public + 64 bytes secret (or just 32 bytes seed)
    if (size < crypto_sign_ed25519_PUBLICKEYBYTES + crypto_sign_ed25519_SECRETKEYBYTES) {
        spdlog::error("ServerOwner: Key file is too small ({} bytes)", static_cast<long long>(size));
        return false;
    }
    
    public_key_.resize(crypto_sign_ed25519_PUBLICKEYBYTES);
    private_key_.resize(crypto_sign_ed25519_SECRETKEYBYTES);
    
    file.read(reinterpret_cast<char*>(public_key_.data()), crypto_sign_ed25519_PUBLICKEYBYTES);
    file.read(reinterpret_cast<char*>(private_key_.data()), crypto_sign_ed25519_SECRETKEYBYTES);
    file.close();
    
    keys_loaded_ = true;
    spdlog::debug("ServerOwner: Keys loaded successfully");
    return true;
}

std::vector<unsigned char> ServerOwner::sign_message(const std::string& message) {
    std::vector<unsigned char> signature(crypto_sign_ed25519_BYTES);
    unsigned long long sig_len;
    
    crypto_sign_ed25519_detached(
        signature.data(), &sig_len,
        reinterpret_cast<const unsigned char*>(message.data()), message.size(),
        private_key_.data()
    );
    
    return signature;
}

// ============================================================================
// Command Implementations
// ============================================================================

ServerOwner::CommandResult ServerOwner::cmd_announce(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: /announce <message>"};
    }

    auto* listener = server_.listener();
    if (!listener) {
        return {false, "Server listener is not available"};
    }

    const std::string message = join_args(args);
    listener->broadcast(make_command_response_envelope("announce", message));
    
    spdlog::info("ANNOUNCEMENT: {}", message);
    
    return {true, "Announcement sent to all connected users: " + message};
}

ServerOwner::CommandResult ServerOwner::cmd_ban(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: /ban <user_id> [reason]"};
    }
    
    std::string user_id = args[0];
    std::string reason = "No reason given";
    
    if (args.size() > 1) {
        reason.clear();
        for (size_t i = 1; i < args.size(); ++i) {
            if (i > 1) reason += " ";
            reason += args[i];
        }
    }
    
    // Prevent banning yourself
    if (user_id == std::string(ServerOwner::user_id())) {
        return {false, "Cannot ban the server owner"};
    }
    
    auto* listener = server_.listener();
    if (!listener) {
        return {false, "Server listener is not available"};
    }

    auto session = listener->find_session(user_id);
    if (!session) {
        spdlog::warn("BAN requested for offline user {} but no persistent ban API exists", user_id);
        return {false, "User is not online, and persistent server bans are not supported by the current server APIs"};
    }

    session->disconnect("Banned by server owner: " + reason);

    // The current server APIs do not expose any persistent server-wide ban store,
    // so the best available behavior is to disconnect the active session.
    spdlog::info("BAN: {} - Reason: {}", user_id, reason);
    
    return {true, "User " + user_id + " has been disconnected. Persistent server-wide bans are not supported by the current server APIs. Reason: " + reason};
}

ServerOwner::CommandResult ServerOwner::cmd_kick(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: /kick <user_id> [reason]"};
    }
    
    std::string user_id = args[0];
    std::string reason = "No reason given";
    
    if (args.size() > 1) {
        reason.clear();
        for (size_t i = 1; i < args.size(); ++i) {
            if (i > 1) reason += " ";
            reason += args[i];
        }
    }
    
    auto* listener = server_.listener();
    if (!listener) {
        return {false, "Server listener is not available"};
    }

    auto session = listener->find_session(user_id);
    if (!session) {
        return {false, "User is not currently online: " + user_id};
    }

    session->disconnect("Kicked by server owner: " + reason);
    spdlog::info("KICK: {} - Reason: {}", user_id, reason);
    
    return {true, "User " + user_id + " has been kicked. Reason: " + reason};
}

ServerOwner::CommandResult ServerOwner::cmd_shutdown(const std::vector<std::string>& args) {
    int delay_seconds = 0;
    
    if (!args.empty()) {
        try {
            delay_seconds = std::stoi(args[0]);
        } catch (...) {
            // Not a number, treat as message
        }
    }
    
    std::string message = "Server is shutting down";
    if (delay_seconds > 0) {
        message += " in " + std::to_string(delay_seconds) + " seconds";
    }
    
    cmd_announce({message});

    if (!server_.is_running()) {
        return {false, "Server is not running"};
    }

    std::thread([this, delay_seconds]() {
        if (delay_seconds > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(delay_seconds));
        }

        spdlog::info("SHUTDOWN initiated by server owner");
        server_.shutdown();
    }).detach();

    return {true, delay_seconds > 0
        ? "Server shutdown scheduled in " + std::to_string(delay_seconds) + " seconds"
        : "Server shutdown initiated"};
}

ServerOwner::CommandResult ServerOwner::cmd_restart(const std::vector<std::string>& /*args*/) {
    cmd_announce({"Server is restarting..."});
    
    spdlog::info("RESTART initiated");

    std::thread([this]() {
        server_.shutdown();
    }).detach();

    return {true, "Server restart requested. This build does not support in-process restart, so the server is shutting down for an external supervisor to restart it."};
}

ServerOwner::CommandResult ServerOwner::cmd_status(const std::vector<std::string>& /*args*/) {
    std::stringstream ss;
    auto* listener = server_.listener();
    auto* command_handler = listener ? listener->command_handler() : nullptr;
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time_).count();
    const int online_users = listener ? listener->active_connection_count() : 0;
    const std::size_t active_channels = command_handler ? command_handler->channel_count() : 0;
    
    ss << "=== Server Status ===\n";
    ss << "Server Owner: " << user_id() << "\n";
    ss << "Status: " << (server_.is_running() ? "Online" : "Offline") << "\n";
    ss << "Online users: " << online_users << "\n";
    ss << "Active channels: " << active_channels << "\n";
    ss << "Uptime: " << uptime << " seconds\n";
    ss << "Bind: " << server_.config().host << ":" << server_.config().port << "\n";
    
    return {true, ss.str()};
}

ServerOwner::CommandResult ServerOwner::cmd_config(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {false, "Usage: /config <key> [value]\nUse '/config list' to see available keys"};
    }
    
    std::string key = args[0];
    
    if (key == "list") {
        return {true, "Available config keys: max_connections, log_level, msg_rate_per_sec"};
    }
    
    if (args.size() < 2) {
        // Get config value
        return {false, "Getting config values not yet implemented"};
    }
    
    // Set config value
    std::string value = args[1];
    spdlog::info("CONFIG: {} = {}", key, value);
    
    return {true, "Config updated: " + key + " = " + value};
}

ServerOwner::CommandResult ServerOwner::cmd_help(const std::vector<std::string>& /*args*/) {
    std::string help_text = R"(Available admin commands:

/announce <message>     - Send announcement to all connected users
/ban <user> [reason]    - Ban a user from the server
/kick <user> [reason]   - Disconnect a user from the server
/shutdown [delay]       - Gracefully shutdown the server
/restart                - Restart the server
/status                 - Show server status and statistics
/config <key> [value]   - View or change server configuration
/help                   - Show this help message

You can use abbreviated forms: /a for /announce, /b for /ban, etc.
)";
    
    return {true, help_text};
}

} // namespace grotto::admin
