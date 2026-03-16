#include "grotto/tui/admin_socket_listener.hpp"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace grotto::tui {

// ── Static helpers ──────────────────────────────

std::string AdminSocketListener::socket_path()
{
#ifdef _WIN32
    return R"(\\.\pipe\grotto-admin)";
#else
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg) + "/grotto-admin.sock";
    }
    return "/tmp/grotto-admin.sock";
#endif
}

// ── Construction / destruction ──────────────────

AdminSocketListener::AdminSocketListener(boost::asio::io_context& ioc)
    : ioc_(ioc)
    , read_buf_(kReadBufSize)
{
#ifdef _WIN32
    pipe_name_ = socket_path();
#endif
}

AdminSocketListener::~AdminSocketListener()
{
    stop();
}

// ── Public interface ────────────────────────────

void AdminSocketListener::set_command_callback(CommandCallback cb)
{
    on_command_ = std::move(cb);
}

bool AdminSocketListener::has_client() const
{
    return has_client_.load();
}

void AdminSocketListener::start()
{
    running_ = true;

#ifdef _WIN32
    create_pipe();
#else
    const auto path = socket_path();
    ::unlink(path.c_str());

    acceptor_ = std::make_unique<unix_socket::acceptor>(
        ioc_, unix_socket::endpoint(path));
    client_socket_ = std::make_unique<unix_socket::socket>(ioc_);
#endif

    do_accept();
    spdlog::info("AdminSocketListener started on {}", socket_path());
}

void AdminSocketListener::stop()
{
    if (!running_.exchange(false))
        return;

#ifdef _WIN32
    if (client_stream_) {
        boost::system::error_code ec;
        client_stream_->close(ec);
        client_stream_.reset();
    }
    if (pipe_handle_ && pipe_handle_ != INVALID_HANDLE_VALUE) {
        ::DisconnectNamedPipe(pipe_handle_);
        ::CloseHandle(pipe_handle_);
        pipe_handle_ = nullptr;
    }
#else
    if (acceptor_) {
        boost::system::error_code ec;
        acceptor_->close(ec);
    }
    if (client_socket_) {
        boost::system::error_code ec;
        client_socket_->close(ec);
    }
    ::unlink(socket_path().c_str());
#endif

    has_client_ = false;
    spdlog::info("AdminSocketListener stopped");
}

// ── Platform-specific accept ────────────────────

#ifdef _WIN32

void AdminSocketListener::create_pipe()
{
    pipe_handle_ = ::CreateNamedPipeA(
        pipe_name_.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,              // max instances
        kReadBufSize,   // out buffer
        kReadBufSize,   // in buffer
        0,              // default timeout
        nullptr         // default security
    );

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        spdlog::error("CreateNamedPipe failed: {}", ::GetLastError());
        return;
    }
}

void AdminSocketListener::do_accept()
{
    if (!running_) return;

    // ConnectNamedPipe in overlapped mode via stream_handle
    // We wrap the pipe handle in a stream_handle for async I/O
    client_stream_ = std::make_unique<boost::asio::windows::stream_handle>(
        ioc_, pipe_handle_);

    // Use an OVERLAPPED structure for async ConnectNamedPipe
    auto overlapped = std::make_shared<OVERLAPPED>();
    std::memset(overlapped.get(), 0, sizeof(OVERLAPPED));
    overlapped->hEvent = ::CreateEventA(nullptr, TRUE, FALSE, nullptr);

    BOOL connected = ::ConnectNamedPipe(pipe_handle_, overlapped.get());
    DWORD err = ::GetLastError();

    if (connected || err == ERROR_PIPE_CONNECTED) {
        // Client already connected
        ::CloseHandle(overlapped->hEvent);
        has_client_ = true;
        spdlog::info("Admin TUI client connected (immediate)");
        do_read();
        return;
    }

    if (err != ERROR_IO_PENDING) {
        spdlog::error("ConnectNamedPipe failed: {}", err);
        ::CloseHandle(overlapped->hEvent);
        return;
    }

    // Wait asynchronously for the client to connect
    auto self = shared_from_this();
    auto wait_handle = std::make_shared<boost::asio::windows::object_handle>(
        ioc_, overlapped->hEvent);

    wait_handle->async_wait(
        [this, self, wait_handle, overlapped](const boost::system::error_code& ec) {
            ::CloseHandle(overlapped->hEvent);
            if (ec || !running_) {
                if (ec && running_)
                    spdlog::error("Admin pipe wait error: {}", ec.message());
                return;
            }
            has_client_ = true;
            spdlog::info("Admin TUI client connected");
            do_read();
        });
}

#else // Linux / Unix

void AdminSocketListener::do_accept()
{
    if (!running_) return;

    auto self = shared_from_this();
    acceptor_->async_accept(*client_socket_,
        [this, self](const boost::system::error_code& ec) {
            if (ec) {
                if (running_)
                    spdlog::error("Admin accept error: {}", ec.message());
                return;
            }
            has_client_ = true;
            spdlog::info("Admin TUI client connected");
            do_read();
        });
}

#endif

// ── Async read loop ─────────────────────────────

void AdminSocketListener::do_read()
{
    if (!running_ || !has_client_) return;

    auto self = shared_from_this();

    // Step 1: read 4-byte length prefix
    auto len_buf = std::make_shared<std::array<uint8_t, 4>>();

#ifdef _WIN32
    boost::asio::async_read(*client_stream_,
#else
    boost::asio::async_read(*client_socket_,
#endif
        boost::asio::buffer(*len_buf),
        [this, self, len_buf](const boost::system::error_code& ec, size_t /*bytes*/) {
            if (ec) {
                if (running_ && ec != boost::asio::error::operation_aborted) {
                    spdlog::info("Admin client disconnected (read header): {}", ec.message());
                }
                on_client_disconnect();
                return;
            }

            uint32_t msg_len = protocol::decode_frame_length(len_buf->data(), 4);
            if (msg_len == 0 || msg_len > kMaxMsgSize) {
                spdlog::warn("Admin: invalid message length {}", msg_len);
                on_client_disconnect();
                return;
            }

            // Step 2: read payload
            auto payload_buf = std::make_shared<std::vector<uint8_t>>(msg_len);

#ifdef _WIN32
            boost::asio::async_read(*client_stream_,
#else
            boost::asio::async_read(*client_socket_,
#endif
                boost::asio::buffer(*payload_buf),
                [this, self, payload_buf](const boost::system::error_code& ec2, size_t /*bytes*/) {
                    if (ec2) {
                        if (running_ && ec2 != boost::asio::error::operation_aborted) {
                            spdlog::info("Admin client disconnected (read payload): {}",
                                         ec2.message());
                        }
                        on_client_disconnect();
                        return;
                    }

                    std::string payload(
                        reinterpret_cast<const char*>(payload_buf->data()),
                        payload_buf->size());
                    process_message(payload);

                    // Continue reading
                    do_read();
                });
        });
}

// ── Message processing ──────────────────────────

void AdminSocketListener::process_message(const std::string& payload)
{
    try {
        auto j = nlohmann::json::parse(payload);
        if (on_command_) {
            on_command_(j);
        }
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::warn("Admin: invalid JSON from client: {}", e.what());
    }
}

// ── Send event to connected client ──────────────

void AdminSocketListener::send_event(const nlohmann::json& event)
{
    if (!has_client_ || !running_) return;

    auto frame = protocol::encode_frame(event);

    std::lock_guard lock(write_mutex_);

    auto buf = std::make_shared<std::vector<uint8_t>>(std::move(frame));
    auto self = shared_from_this();

#ifdef _WIN32
    boost::asio::async_write(*client_stream_,
#else
    boost::asio::async_write(*client_socket_,
#endif
        boost::asio::buffer(*buf),
        [this, self, buf](const boost::system::error_code& ec, size_t /*bytes*/) {
            if (ec) {
                spdlog::warn("Admin: send_event write error: {}", ec.message());
                on_client_disconnect();
            }
        });
}

// ── Client disconnect handling ──────────────────

void AdminSocketListener::on_client_disconnect()
{
    has_client_ = false;

#ifdef _WIN32
    if (client_stream_) {
        boost::system::error_code ec;
        client_stream_->close(ec);
        client_stream_.reset();
    }
    if (pipe_handle_ && pipe_handle_ != INVALID_HANDLE_VALUE) {
        ::DisconnectNamedPipe(pipe_handle_);
        ::CloseHandle(pipe_handle_);
        pipe_handle_ = nullptr;
    }
    // Recreate pipe for next client
    if (running_) {
        create_pipe();
        do_accept();
    }
#else
    if (client_socket_) {
        boost::system::error_code ec;
        client_socket_->close(ec);
        client_socket_ = std::make_unique<unix_socket::socket>(ioc_);
    }
    if (running_) {
        do_accept();
    }
#endif
}

} // namespace grotto::tui
