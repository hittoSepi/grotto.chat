#include "grotto/api/server.hpp"

#include <spdlog/spdlog.h>
#include <boost/beast.hpp>

namespace grotto::api {

namespace beast = boost::beast;
namespace http = beast::http;

// ── Session Implementation ────────────────────────────────────

Session::Session(tcp::socket socket, std::shared_ptr<Server> server)
    : socket_(std::move(socket))
    , server_(std::move(server))
{
}

void Session::start() {
    read_request();
}

void Session::close() {
    beast::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);
}

void Session::read_request() {
    auto self = shared_from_this();
    
    http::async_read(socket_, buffer_, beast_req_,
        [self](beast::error_code ec, std::size_t bytes) {
            self->on_read(ec, bytes);
        });
}

void Session::on_read(beast::error_code ec, std::size_t /*bytes*/) {
    if (ec == http::error::end_of_stream) {
        close();
        return;
    }
    
    if (ec) {
        spdlog::error("[API] Read error: {}", ec.message());
        close();
        return;
    }
    
    // Create Request wrapper
    auto remote = socket_.remote_endpoint();
    std::string remote_addr = remote.address().to_string() + ":" + std::to_string(remote.port());
    
    Request req(std::move(beast_req_), remote_addr);
    
    // Handle request
    auto res = server_->handle_request(req);
    write_response(std::move(res));
}

void Session::write_response(Response&& res) {
    auto beast_res = res.to_beast();
    auto sp = std::make_shared<http::response<http::string_body>>(std::move(beast_res));
    
    auto self = shared_from_this();
    http::async_write(socket_, *sp,
        [self, sp](beast::error_code ec, std::size_t bytes) {
            self->on_write(ec, bytes, sp->need_eof());
        });
}

void Session::on_write(beast::error_code ec, std::size_t /*bytes*/, bool close) {
    if (ec) {
        spdlog::error("[API] Write error: {}", ec.message());
    }
    
    if (close) {
        this->close();
        return;
    }
    
    // Read next request (keep-alive)
    read_request();
}

// ── Server Implementation ─────────────────────────────────────

Server::Server(asio::io_context& io_context, const ServerConfig& config)
    : io_context_(io_context)
    , acceptor_(io_context)
    , config_(config)
{
    // Add configured API keys
    for (const auto& key : config.api_keys) {
        api_keys_.add_key(key);
    }
}

Server::~Server() {
    stop();
}

void Server::start() {
    if (running_.exchange(true)) {
        return; // Already running
    }
    
    // Setup middleware chain
    if (config_.cors_enabled) {
        use(middleware::cors());
    }
    
    if (config_.rate_limit_enabled) {
        use(middleware::rate_limit(config_.rate_limit_requests));
    }
    
    // API key auth (only if keys configured)
    if (!config_.api_keys.empty()) {
        use(middleware::api_key_auth(config_.api_keys));
    }
    
    // Bind and listen
    tcp::resolver resolver(io_context_);
    auto endpoint = tcp::endpoint(
        asio::ip::make_address(config_.bind_address), 
        config_.port
    );
    
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    
    spdlog::info("[API] HTTP server listening on {}:{}", 
        config_.bind_address, config_.port);
    
    accept();
}

void Server::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    
    beast::error_code ec;
    acceptor_.close(ec);
    
    // Close all sessions
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& session : sessions_) {
        session->close();
    }
    sessions_.clear();
    
    spdlog::info("[API] HTTP server stopped");
}

void Server::accept() {
    if (!running_) return;
    
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            on_accept(ec, std::move(socket));
        });
}

void Server::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        if (running_) {
            spdlog::error("[API] Accept error: {}", ec.message());
        }
        return;
    }
    
    if (!running_) return;
    
    // Create and start session
    auto session = std::make_shared<Session>(std::move(socket), shared_from_this());
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.push_back(session);
    }
    session->start();
    
    // Accept next connection
    accept();
}

Response Server::handle_request(const Request& req) {
    // Make mutable copies for middleware
    Request req_copy = req;
    Response res;
    
    // Run middleware chain
    if (!middleware_.process(req_copy, res)) {
        // Middleware rejected request
        return res;
    }
    
    // Route to handler
    auto target = req_copy.target_path();
    auto match = router_.match(req_copy.method(), target);
    
    if (match.found) {
        try {
            return match.handler(req_copy);
        } catch (const std::exception& e) {
            spdlog::error("[API] Handler exception: {}", e.what());
            return Response::error("Internal server error", 500);
        }
    }
    
    // No route found
    return Response::not_found("Endpoint not found");
}

void Server::get(const std::string& path, Handler handler) {
    router_.get(path, std::move(handler));
}

void Server::post(const std::string& path, Handler handler) {
    router_.post(path, std::move(handler));
}

void Server::put(const std::string& path, Handler handler) {
    router_.put(path, std::move(handler));
}

void Server::del(const std::string& path, Handler handler) {
    router_.del(path, std::move(handler));
}

void Server::patch(const std::string& path, Handler handler) {
    router_.patch(path, std::move(handler));
}

void Server::use(MiddlewareFn middleware) {
    middleware_.add(std::move(middleware));
}

} // namespace grotto::api
