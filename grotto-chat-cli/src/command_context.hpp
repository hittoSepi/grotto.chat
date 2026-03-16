#pragma once

#include <grotto/tui/admin_socket_client.hpp>
#include <nlohmann/json.hpp>

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace grotto::cli {

class CommandRegistry;

class CommandContext {
public:
    explicit CommandContext(CommandRegistry& registry);
    ~CommandContext();

    // Parsed arguments (positional, after command name)
    std::vector<std::string> args;

    // Parsed flags (--key value or --flag)
    std::map<std::string, std::string> flags;

    // Reference to registry (for help command)
    CommandRegistry& registry;

    /// Connect to admin socket. Returns false on failure.
    bool connect(const std::string& socket_path = "");

    /// Send a command and wait for a response (with timeout).
    /// Returns nullopt on timeout or connection error.
    std::optional<nlohmann::json> send_and_wait(
        const std::string& cmd,
        const nlohmann::json& params = nlohmann::json::object(),
        int timeout_ms = 5000);

    /// Subscribe to events. Callback runs on I/O thread.
    void subscribe(std::function<void(const nlohmann::json&)> cb);

    /// Print to stdout
    void print(const std::string& msg);

    /// Print to stderr
    void error(const std::string& msg);

    /// Get the underlying admin client (for TUI command)
    tui::AdminSocketClient& admin_client();

    /// Check if connected
    bool connected() const;

private:
    tui::AdminSocketClient client_;

    // For send_and_wait synchronization
    std::mutex response_mutex_;
    std::condition_variable response_cv_;
    std::optional<nlohmann::json> pending_response_;
};

} // namespace grotto::cli
