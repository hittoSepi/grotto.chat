#pragma once

#include "grotto/api/router.hpp"
#include "grotto/api/middleware.hpp"
#include "grotto/api/auth.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <spdlog/logger.h>

#include <memory>
#include <atomic>

namespace grotto::api {

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

// HTTP API Server configuration
struct ServerConfig {
    uint16_t port = 8080;
    std::string bind_address = "0.0.0.0";
    bool cors_enabled = true;
    bool rate_limit_enabled = true;
    int rate_limit_requests = 60;  // per minute
    
    // API keys (empty = no auth required, not recommended for production)
    std::vector<std::string> api_keys;
};

// Forward declarations
class Session;

// HTTP API Server
class Server : public std::enable_shared_from_this<Server> {
public:
    Server(asio::io_context& io_context, const ServerConfig& config = {});
    ~Server();

    // Start listening
    void start();
    
    // Stop server
    void stop();
    
    // Check if running
    bool is_running() const { return running_.load(); }
    
    // Route registration (convenience methods)
    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);
    void put(const std::string& path, Handler handler);
    void del(const std::string& path, Handler handler);
    void patch(const std::string& path, Handler handler);
    
    // Add middleware
    void use(MiddlewareFn middleware);
    
    // Access components
    Router& router() { return router_; }
    const Router& router() const { return router_; }
    
    ApiKeyManager& api_keys() { return api_keys_; }
    
    // Handle a request (called by Session)
    Response handle_request(const Request& req);

private:
    void accept();
    void on_accept(beast::error_code ec, tcp::socket socket);
    
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    ServerConfig config_;
    
    Router router_;
    MiddlewareChain middleware_;
    ApiKeyManager api_keys_;
    
    std::atomic<bool> running_{false};
    std::vector<std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;
    
    std::shared_ptr<spdlog::logger> logger_;
};

// HTTP Session (connection)
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, std::shared_ptr<Server> server);
    
    void start();
    void close();

private:
    void read_request();
    void on_read(beast::error_code ec, std::size_t bytes);
    void write_response(Response&& res);
    void on_write(beast::error_code ec, std::size_t bytes, bool close);
    
    tcp::socket socket_;
    std::shared_ptr<Server> server_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> beast_req_;
};

} // namespace grotto::api
