#pragma once

#include "grotto/api/types.hpp"
#include "grotto/api/request.hpp"
#include "grotto/api/response.hpp"

#include <functional>
#include <map>
#include <regex>
#include <vector>

namespace grotto::api {

// Route handler type
using Handler = std::function<Response(const Request&)>;

// Route definition
struct Route {
    HttpMethod method;
    std::string path;           // e.g., "/api/v1/channels/{id}"
    std::regex path_regex;      // Compiled regex
    std::vector<std::string> param_names;
    Handler handler;
};

// Route parameters from URL
using RouteParams = std::unordered_map<std::string, std::string>;

// Router for matching requests to handlers
class Router {
public:
    // Register handlers
    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);
    void put(const std::string& path, Handler handler);
    void del(const std::string& path, Handler handler);
    void patch(const std::string& path, Handler handler);
    
    // Match request to handler
    struct MatchResult {
        bool found = false;
        Handler handler;
        RouteParams params;
    };
    
    MatchResult match(HttpMethod method, const std::string& path) const;
    
    // Get all registered routes
    std::vector<Route> routes() const;
    
private:
    void add_route(HttpMethod method, const std::string& path, Handler handler);
    
    std::vector<Route> routes_;
};

} // namespace grotto::api
