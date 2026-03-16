#pragma once

#include "grotto/api/request.hpp"
#include "grotto/api/response.hpp"

#include <functional>
#include <vector>

namespace grotto::api {

// Middleware function type
// Returns true if request should continue, false to stop
using MiddlewareFn = std::function<bool(Request& req, Response& res)>;

// Middleware chain
class MiddlewareChain {
public:
    void add(MiddlewareFn middleware);
    
    // Process request through all middleware
    // Returns true if all passed, false if rejected
    bool process(Request& req, Response& res) const;
    
    void clear();
    
private:
    std::vector<MiddlewareFn> middlewares_;
};

// Built-in middleware
namespace middleware {
    // CORS headers
    MiddlewareFn cors(const std::string& origin = "*");
    
    // Rate limiting (simple in-memory)
    MiddlewareFn rate_limit(int requests_per_minute);
    
    // API key authentication
    MiddlewareFn api_key_auth(const std::vector<std::string>& valid_keys);
    
    // Request logging
    MiddlewareFn logger();
    
    // Content-Type validation
    MiddlewareFn require_json();
}

} // namespace grotto::api
