#pragma once

#include <grotto/tui/admin_socket_listener.hpp>
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <memory>

namespace grotto::tui {

template<typename Mutex>
class TuiLogSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit TuiLogSink(std::shared_ptr<AdminSocketListener> listener)
        : listener_(std::move(listener)) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (!listener_ || !listener_->has_client()) return;

        protocol::LogEntry entry;
        auto lv = spdlog::level::to_string_view(msg.level);
        entry.level = std::string(lv.data(), lv.size());
        entry.msg = std::string(msg.payload.begin(), msg.payload.end());

        // Format timestamp as ISO 8601
        auto time_t = std::chrono::system_clock::to_time_t(msg.time);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t));
        entry.ts = buf;

        listener_->send_event({
            {"type", "log"},
            {"data", entry}
        });
    }

    void flush_() override {}

private:
    std::shared_ptr<AdminSocketListener> listener_;
};

using TuiLogSinkMt = TuiLogSink<std::mutex>;

} // namespace grotto::tui
