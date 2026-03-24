#include "grotto/api/router.hpp"

#include <spdlog/spdlog.h>

namespace grotto::api {

// Convert /api/v1/channels/{id} to regex: /api/v1/channels/([^/]+)
static std::pair<std::regex, std::vector<std::string>> 
parse_route_pattern(const std::string& pattern) {
    std::string regex_str = "^";
    std::vector<std::string> param_names;
    
    size_t pos = 0;
    while (pos < pattern.size()) {
        auto brace_open = pattern.find('{', pos);
        if (brace_open == std::string::npos) {
            // No more params
            regex_str += pattern.substr(pos);
            break;
        }
        
        // Add literal part before param
        regex_str += pattern.substr(pos, brace_open - pos);
        
        // Find closing brace
        auto brace_close = pattern.find('}', brace_open);
        if (brace_close == std::string::npos) {
            // Malformed, treat rest as literal
            regex_str += pattern.substr(brace_open);
            break;
        }
        
        // Extract param name
        auto param_name = pattern.substr(brace_open + 1, brace_close - brace_open - 1);
        param_names.push_back(param_name);
        
        // Add capture group
        regex_str += "([^/]+)";
        
        pos = brace_close + 1;
    }
    
    regex_str += "$";
    return {std::regex(regex_str), param_names};
}

void Router::get(const std::string& path, Handler handler) {
    add_route(HttpMethod::GET, path, std::move(handler));
}

void Router::post(const std::string& path, Handler handler) {
    add_route(HttpMethod::POST, path, std::move(handler));
}

void Router::put(const std::string& path, Handler handler) {
    add_route(HttpMethod::PUT, path, std::move(handler));
}

void Router::del(const std::string& path, Handler handler) {
    add_route(HttpMethod::DELETE, path, std::move(handler));
}

void Router::patch(const std::string& path, Handler handler) {
    add_route(HttpMethod::PATCH, path, std::move(handler));
}

void Router::add_route(HttpMethod method, const std::string& path, Handler handler) {
    auto [regex, params] = parse_route_pattern(path);
    
    routes_.push_back(Route{
        method,
        path,
        std::move(regex),
        std::move(params),
        std::move(handler)
    });
    
    spdlog::debug("[API] Registered route: {} {}", method_to_string(method), path);
}

Router::MatchResult Router::match(HttpMethod method, const std::string& path) const {
    for (const auto& route : routes_) {
        if (route.method != method) continue;
        
        std::smatch match;
        if (std::regex_match(path, match, route.path_regex)) {
            MatchResult result;
            result.found = true;
            result.handler = route.handler;
            
            // Extract params
            for (size_t i = 0; i < route.param_names.size(); ++i) {
                if (i + 1 < match.size()) {
                    result.params[route.param_names[i]] = match[i + 1].str();
                }
            }
            
            return result;
        }
    }
    
    return MatchResult{}; // Not found
}

std::vector<Route> Router::routes() const {
    return routes_;
}

} // namespace grotto::api
