#include "grotto/api/types.hpp"

#include <algorithm>

namespace grotto::api {

HttpMethod parse_method(const std::string& method) {
    std::string m = method;
    std::transform(m.begin(), m.end(), m.begin(), ::toupper);
    
    if (m == "GET") return HttpMethod::GET;
    if (m == "POST") return HttpMethod::POST;
    if (m == "PUT") return HttpMethod::PUT;
    if (m == "DELETE") return HttpMethod::DELETE;
    if (m == "PATCH") return HttpMethod::PATCH;
    if (m == "HEAD") return HttpMethod::HEAD;
    if (m == "OPTIONS") return HttpMethod::OPTIONS;
    
    return HttpMethod::UNKNOWN;
}

std::string method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

QueryParams parse_query(const std::string& query) {
    QueryParams result;
    
    std::size_t pos = 0;
    while (pos < query.size()) {
        auto amp = query.find('&', pos);
        auto part = query.substr(pos, amp - pos);
        
        auto eq = part.find('=');
        if (eq != std::string::npos) {
            auto key = part.substr(0, eq);
            auto value = part.substr(eq + 1);
            result[key] = value;
        } else if (!part.empty()) {
            result[part] = "";
        }
        
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    
    return result;
}

} // namespace grotto::api
