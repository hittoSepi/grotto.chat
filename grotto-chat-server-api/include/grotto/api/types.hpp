#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <optional>

namespace grotto::api {

// HTTP methods
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS,
    UNKNOWN
};

// Convert string to method
HttpMethod parse_method(const std::string& method);
std::string method_to_string(HttpMethod method);

// Query parameters
using QueryParams = std::unordered_map<std::string, std::string>;
using Headers = std::unordered_map<std::string, std::string>;

// Parse query string
QueryParams parse_query(const std::string& query);

} // namespace grotto::api
