#include "grotto/api/middleware.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace grotto::api {

void MiddlewareChain::add(MiddlewareFn middleware) {
    middlewares_.push_back(std::move(middleware));
}

bool MiddlewareChain::process(Request& req, Response& res) const {
    for (const auto& mw : middlewares_) {
        if (!mw(req, res)) {
            return false;
        }
    }
    return true;
}

void MiddlewareChain::clear() {
    middlewares_.clear();
}

namespace middleware {

MiddlewareFn cors(const std::string& origin) {
    return [origin](Request& req, Response& res) -> bool {
        res.header("Access-Control-Allow-Origin", origin);
        res.header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
        res.header("Access-Control-Allow-Headers", "Authorization, Content-Type");
        
        // Handle preflight
        if (req.method() == HttpMethod::OPTIONS) {
            res.status(204);
            return false; // Stop processing
        }
        
        return true;
    };
}

MiddlewareFn rate_limit(int requests_per_minute) {
    using steady_clock_t = std::chrono::steady_clock;

    struct ClientInfo {
        int count = 0;
        steady_clock_t::time_point window_start{};
        steady_clock_t::time_point last_access{};
    };

    auto last_cleanup = std::make_shared<steady_clock_t::time_point>(steady_clock_t::now());
    
    auto clients = std::make_shared<std::unordered_map<std::string, ClientInfo>>();
    auto mutex = std::make_shared<std::mutex>();
    
    return [clients, mutex, last_cleanup, requests_per_minute](Request& req, Response& res) -> bool {
        auto now = steady_clock_t::now();
        auto addr = req.remote_addr();
        constexpr auto cleanup_interval = std::chrono::minutes(10);
        constexpr auto idle_timeout = std::chrono::minutes(10);
        
        std::lock_guard<std::mutex> lock(*mutex);

        if (now - *last_cleanup >= cleanup_interval) {
            for (auto it = clients->begin(); it != clients->end();) {
                if (now - it->second.last_access >= idle_timeout) {
                    it = clients->erase(it);
                } else {
                    ++it;
                }
            }
            *last_cleanup = now;
        }

        auto& info = (*clients)[addr];
        if (info.window_start == steady_clock_t::time_point{}) {
            info.window_start = now;
        }
        info.last_access = now;
        
        // Check if window expired
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - info.window_start).count();
        if (elapsed >= 1) {
            info.count = 0;
            info.window_start = now;
        }
        
        // Check limit
        if (info.count >= requests_per_minute) {
            res.status(429).json({{"success", false}, {"error", "Rate limit exceeded"}});
            return false;
        }
        
        info.count++;
        return true;
    };
}

MiddlewareFn api_key_auth(const std::vector<std::string>& valid_keys) {
    auto keys = std::make_shared<std::unordered_set<std::string>>(valid_keys.begin(), valid_keys.end());
    
    return [keys](Request& req, Response& res) -> bool {
        auto token = req.bearer_token();
        
        if (!token || keys->find(*token) == keys->end()) {
            res.status(401).json({{"success", false}, {"error", "Invalid or missing API key"}});
            return false;
        }
        
        return true;
    };
}

MiddlewareFn logger() {
    return [](Request& req, Response& res) -> bool {
        spdlog::info("[API] {} {} - {} - {}",
            method_to_string(req.method()),
            req.target_path(),
            req.remote_addr(),
            res.status());
        return true;
    };
}

MiddlewareFn require_json() {
    return [](Request& req, Response& res) -> bool {
        auto content_type = req.header("Content-Type");
        if (content_type && content_type->find("application/json") != std::string::npos) {
            return true;
        }
        
        res.status(400).json({{"success", false}, {"error", "Content-Type must be application/json"}});
        return false;
    };
}

} // namespace middleware

} // namespace grotto::api
