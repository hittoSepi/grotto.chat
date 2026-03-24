#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

#ifdef _WIN32
#include <boost/asio/windows/stream_handle.hpp>
#else
#include <boost/asio/local/stream_protocol.hpp>
#endif

namespace grotto::tui {

using EventCallback = std::function<void(const nlohmann::json& event)>;

class AdminSocketClient {
public:
    AdminSocketClient();
    ~AdminSocketClient();

    // Connect to admin socket. Empty path = default.
    // Returns false if connection fails.
    bool connect(const std::string& path = "");

    void disconnect();
    void send_command(const nlohmann::json& cmd);
    void set_event_callback(EventCallback cb);
    bool connected() const;

    // Default socket path (same convention as listener)
    static std::string default_socket_path();

private:
    void io_thread_func();
    void do_read();
    void process_message(const std::string& payload);

    boost::asio::io_context ioc_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_;
    EventCallback on_event_;
    std::mutex write_mutex_;
    std::thread io_thread_;
    std::atomic<bool> connected_{false};

#ifdef _WIN32
    std::unique_ptr<boost::asio::windows::stream_handle> stream_;
#else
    using unix_socket = boost::asio::local::stream_protocol;
    std::unique_ptr<unix_socket::socket> socket_;
#endif

    std::vector<uint8_t> read_buf_;
    static constexpr size_t kReadBufSize = 65536;
};

} // namespace grotto::tui
