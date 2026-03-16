#include "command_context.hpp"
#include <grotto/tui/admin_protocol.hpp>
#include <iostream>

namespace grotto::cli {

CommandContext::CommandContext(CommandRegistry& reg)
    : registry(reg) {}

CommandContext::~CommandContext() {
    client_.disconnect();
}

bool CommandContext::connect(const std::string& socket_path) {
    return client_.connect(socket_path);
}

std::optional<nlohmann::json> CommandContext::send_and_wait(
    const std::string& cmd,
    const nlohmann::json& params,
    int timeout_ms)
{
    // Set up response listener
    {
        std::lock_guard lock(response_mutex_);
        pending_response_.reset();
    }

    client_.set_event_callback([this, &cmd](const nlohmann::json& event) {
        if (event.value("type", "") == "response" &&
            event.value("cmd", "") == cmd) {
            std::lock_guard lock(response_mutex_);
            pending_response_ = event;
            response_cv_.notify_one();
        }
    });

    // Send the command
    auto msg = tui::protocol::to_json_command(cmd, params);
    client_.send_command(msg);

    // Wait for response
    std::unique_lock lock(response_mutex_);
    if (response_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [this] { return pending_response_.has_value(); })) {
        return pending_response_;
    }
    return std::nullopt;
}

void CommandContext::subscribe(std::function<void(const nlohmann::json&)> cb) {
    client_.set_event_callback(std::move(cb));
}

void CommandContext::print(const std::string& msg) {
    std::cout << msg << std::endl;
}

void CommandContext::error(const std::string& msg) {
    std::cerr << msg << std::endl;
}

tui::AdminSocketClient& CommandContext::admin_client() {
    return client_;
}

bool CommandContext::connected() const {
    return client_.connected();
}

} // namespace grotto::cli
