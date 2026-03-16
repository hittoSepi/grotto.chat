#pragma once

#include "grotto/api/types.hpp"

#include <nlohmann/json.hpp>
#include <boost/beast.hpp>

namespace grotto::api {

namespace beast = boost::beast;
namespace http = beast::http;

using json = nlohmann::json;

// HTTP Request wrapper
class Request {
public:
    Request() = default;
    
    // Construct from Beast request
    explicit Request(http::request<http::string_body>&& req, 
                     const std::string& remote_addr = "");
    
    // HTTP method
    HttpMethod method() const { return method_; }
    
    // Full path including query string
    const std::string& path() const { return path_; }
    
    // Path without query string
    std::string target_path() const;
    
    // Query string (after ?)
    std::string query_string() const;
    
    // Parsed query parameters
    const QueryParams& query() const { return query_; }
    
    // Get specific query parameter
    std::optional<std::string> query_param(const std::string& key) const;
    
    // All headers
    const Headers& headers() const { return headers_; }
    
    // Get specific header
    std::optional<std::string> header(const std::string& key) const;
    
    // Raw body
    const std::string& body() const { return body_; }
    
    // Parsed JSON body (throws if not JSON)
    json json_body() const;
    
    // Check if body is valid JSON
    bool has_json_body() const;
    
    // Remote address
    const std::string& remote_addr() const { return remote_addr_; }
    
    // Authentication
    std::optional<std::string> bearer_token() const;
    
private:
    HttpMethod method_ = HttpMethod::UNKNOWN;
    std::string path_;
    std::string body_;
    std::string remote_addr_;
    QueryParams query_;
    Headers headers_;
};

} // namespace grotto::api
