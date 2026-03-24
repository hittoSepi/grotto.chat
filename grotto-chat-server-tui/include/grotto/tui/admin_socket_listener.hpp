#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

#ifdef _WIN32
#include <boost/asio/windows/stream_handle.hpp>
#include <boost/asio/windows/object_handle.hpp>
#else
#include <boost/asio/local/stream_protocol.hpp>
#endif

namespace grotto::tui {

using CommandCallback = std::function<void(const nlohmann::json& cmd)>;

class AdminSocketListener : public std::enable_shared_from_this<AdminSocketListener> {
public:
    explicit AdminSocketListener(boost::asio::io_context& ioc);
    ~AdminSocketListener();

    void start();
    void stop();
    void send_event(const nlohmann::json& event);
    void set_command_callback(CommandCallback cb);
    bool has_client() const;
    static std::string socket_path();

private:
    void do_accept();
    void do_read();
    void process_message(const std::string& payload);
    void on_client_disconnect();

    boost::asio::io_context& ioc_;
    CommandCallback on_command_;
    std::mutex write_mutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> has_client_{false};

#ifdef _WIN32
    std::string pipe_name_;
    void* pipe_handle_ = nullptr;  // HANDLE
    std::unique_ptr<boost::asio::windows::stream_handle> client_stream_;
    void create_pipe();
#else
    using unix_socket = boost::asio::local::stream_protocol;
    std::unique_ptr<unix_socket::acceptor> acceptor_;
    std::unique_ptr<unix_socket::socket> client_socket_;
#endif

    std::vector<uint8_t> read_buf_;
    static constexpr size_t kReadBufSize = 65536;
    static constexpr size_t kMaxMsgSize = 65536;
};

} // namespace grotto::tui
