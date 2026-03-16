#include "grotto/api/request.hpp"

#include <algorithm>

namespace grotto::api {

Request::Request(http::request<http::string_body>&& req, const std::string& remote_addr)
    : method_(parse_method(std::string(req.method_string())))
    , path_(req.target())
    , body_(req.body())
    , remote_addr_(remote_addr)
{
    // Parse query string
    auto qpos = path_.find('?');
    if (qpos != std::string::npos) {
        query_ = parse_query(path_.substr(qpos + 1));
    }
    
    // Copy headers
    for (const auto& field : req) {
        headers_[std::string(field.name_string())] = std::string(field.value());
    }
}

std::string Request::target_path() const {
    auto qpos = path_.find('?');
    if (qpos == std::string::npos) return path_;
    return path_.substr(0, qpos);
}

std::string Request::query_string() const {
    auto qpos = path_.find('?');
    if (qpos == std::string::npos) return "";
    return path_.substr(qpos + 1);
}

std::optional<std::string> Request::query_param(const std::string& key) const {
    auto it = query_.find(key);
    if (it != query_.end()) return it->second;
    return std::nullopt;
}

std::optional<std::string> Request::header(const std::string& key) const {
    auto it = headers_.find(key);
    if (it != headers_.end()) return it->second;
    return std::nullopt;
}

json Request::json_body() const {
    return json::parse(body_);
}

bool Request::has_json_body() const {
    try {
        json::parse(body_);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::string> Request::bearer_token() const {
    auto auth = header("Authorization");
    if (!auth) return std::nullopt;
    
    const std::string prefix = "Bearer ";
    if (auth->size() > prefix.size() && 
        auth->substr(0, prefix.size()) == prefix) {
        return auth->substr(prefix.size());
    }
    return std::nullopt;
}

} // namespace grotto::api
