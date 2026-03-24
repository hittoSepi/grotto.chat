#include "grotto/tui/admin_socket_client.hpp"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace grotto::tui {

// ── Default socket path ────────────────────────

std::string AdminSocketClient::default_socket_path()
{
#ifdef _WIN32
    return R"(\\.\pipe\grotto-admin)";
#else
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] != '\0')
        return std::string(xdg) + "/grotto-admin.sock";
    return "/tmp/grotto-admin.sock";
#endif
}

// ── Construction / destruction ─────────────────

AdminSocketClient::AdminSocketClient()
    : read_buf_(kReadBufSize)
{
}

AdminSocketClient::~AdminSocketClient()
{
    disconnect();
}

// ── Connect ────────────────────────────────────

bool AdminSocketClient::connect(const std::string& path)
{
    const std::string sock_path = path.empty() ? default_socket_path() : path;

    try {
#ifdef _WIN32
        // Open named pipe as a client
        HANDLE pipe = CreateFileA(
            sock_path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,             // no sharing
            nullptr,       // default security
            OPEN_EXISTING, // must already exist
            FILE_FLAG_OVERLAPPED, // needed for async Boost.Asio
            nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            spdlog::error("AdminSocketClient: CreateFileA('{}') failed, error={}",
                          sock_path, GetLastError());
            return false;
        }

        // Set pipe to message mode (reading)
        DWORD mode = PIPE_READMODE_BYTE;
        SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

        stream_ = std::make_unique<boost::asio::windows::stream_handle>(ioc_, pipe);
#else
        socket_ = std::make_unique<unix_socket::socket>(ioc_);
        unix_socket::endpoint ep(sock_path);
        socket_->connect(ep);
#endif
    } catch (const std::exception& ex) {
        spdlog::error("AdminSocketClient: connect('{}') failed: {}", sock_path, ex.what());
        return false;
    }

    connected_ = true;
    work_.emplace(boost::asio::make_work_guard(ioc_));
    io_thread_ = std::thread(&AdminSocketClient::io_thread_func, this);

    // Kick off the async read loop
    do_read();

    spdlog::info("AdminSocketClient: connected to {}", sock_path);
    return true;
}

// ── Disconnect ─────────────────────────────────

void AdminSocketClient::disconnect()
{
    if (!connected_.exchange(false))
        return;

    boost::system::error_code ec;
#ifdef _WIN32
    if (stream_) {
        stream_->close(ec);
        stream_.reset();
    }
#else
    if (socket_) {
        socket_->close(ec);
        socket_.reset();
    }
#endif

    work_.reset();
    ioc_.stop();

    if (io_thread_.joinable())
        io_thread_.join();

    // Allow re-use after disconnect
    ioc_.restart();

    spdlog::info("AdminSocketClient: disconnected");
}

// ── IO thread ──────────────────────────────────

void AdminSocketClient::io_thread_func()
{
    try {
        ioc_.run();
    } catch (const std::exception& ex) {
        spdlog::error("AdminSocketClient: io_context error: {}", ex.what());
        connected_ = false;
    }
}

// ── Async read loop ────────────────────────────

void AdminSocketClient::do_read()
{
    if (!connected_)
        return;

    // Step 1: read 4-byte length prefix
    auto len_buf = std::make_shared<std::array<uint8_t, 4>>();

#ifdef _WIN32
    auto& stream_ref = *stream_;
#else
    auto& stream_ref = *socket_;
#endif

    boost::asio::async_read(
        stream_ref,
        boost::asio::buffer(*len_buf),
        [this, len_buf](boost::system::error_code ec, std::size_t /*bytes*/) {
            if (ec) {
                if (connected_) {
                    spdlog::warn("AdminSocketClient: read length error: {}", ec.message());
                    connected_ = false;
                }
                return;
            }

            uint32_t payload_len = protocol::decode_frame_length(len_buf->data(), 4);
            if (payload_len == 0 || payload_len > kReadBufSize) {
                spdlog::error("AdminSocketClient: invalid frame length: {}", payload_len);
                connected_ = false;
                return;
            }

            // Step 2: read payload
            auto payload_buf = std::make_shared<std::vector<uint8_t>>(payload_len);

#ifdef _WIN32
            auto& s = *stream_;
#else
            auto& s = *socket_;
#endif

            boost::asio::async_read(
                s,
                boost::asio::buffer(*payload_buf),
                [this, payload_buf](boost::system::error_code ec2, std::size_t /*bytes*/) {
                    if (ec2) {
                        if (connected_) {
                            spdlog::warn("AdminSocketClient: read payload error: {}", ec2.message());
                            connected_ = false;
                        }
                        return;
                    }

                    std::string payload(payload_buf->begin(), payload_buf->end());
                    process_message(payload);

                    // Continue reading
                    do_read();
                });
        });
}

// ── Process incoming message ───────────────────

void AdminSocketClient::process_message(const std::string& payload)
{
    try {
        auto j = nlohmann::json::parse(payload);
        if (on_event_)
            on_event_(j);
    } catch (const nlohmann::json::parse_error& ex) {
        spdlog::warn("AdminSocketClient: JSON parse error: {}", ex.what());
    }
}

// ── Send command ───────────────────────────────

void AdminSocketClient::send_command(const nlohmann::json& cmd)
{
    std::lock_guard lock(write_mutex_);

    if (!connected_) {
        spdlog::warn("AdminSocketClient: send_command called while disconnected");
        return;
    }

    auto frame = std::make_shared<std::vector<uint8_t>>(protocol::encode_frame(cmd));

#ifdef _WIN32
    auto& stream_ref = *stream_;
#else
    auto& stream_ref = *socket_;
#endif

    boost::asio::async_write(
        stream_ref,
        boost::asio::buffer(*frame),
        [frame](boost::system::error_code ec, std::size_t /*bytes*/) {
            if (ec)
                spdlog::warn("AdminSocketClient: write error: {}", ec.message());
        });
}

// ── Event callback ─────────────────────────────

void AdminSocketClient::set_event_callback(EventCallback cb)
{
    on_event_ = std::move(cb);
}

bool AdminSocketClient::connected() const
{
    return connected_.load();
}

} // namespace grotto::tui
