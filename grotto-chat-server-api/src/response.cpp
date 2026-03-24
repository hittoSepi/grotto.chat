#include "grotto/api/response.hpp"

namespace grotto::api {

Response::Response() = default;

Response& Response::status(int code) {
    status_code_ = code;
    return *this;
}

Response& Response::header(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

Response& Response::content_type(const std::string& type) {
    headers_["Content-Type"] = type;
    return *this;
}

Response& Response::body(const std::string& body) {
    body_ = body;
    return *this;
}

Response& Response::json(const nlohmann::json& data) {
    body_ = data.dump();
    headers_["Content-Type"] = "application/json";
    return *this;
}

Response Response::ok(const nlohmann::json& data) {
    return Response().status(200).json({{"success", true}, {"data", data}});
}

Response Response::created(const nlohmann::json& data) {
    return Response().status(201).json({{"success", true}, {"data", data}});
}

Response Response::no_content() {
    return Response().status(204);
}

Response Response::bad_request(const std::string& message) {
    nlohmann::json error = {{"success", false}, {"error", {{"code", "BAD_REQUEST"}, {"message", message}}}};
    return Response().status(400).json(error);
}

Response Response::unauthorized(const std::string& message) {
    nlohmann::json error = {{"success", false}, {"error", {{"code", "UNAUTHORIZED"}, {"message", message}}}};
    return Response().status(401).json(error);
}

Response Response::not_found(const std::string& message) {
    nlohmann::json error = {{"success", false}, {"error", {{"code", "NOT_FOUND"}, {"message", message}}}};
    return Response().status(404).json(error);
}

Response Response::error(const std::string& message, int code) {
    nlohmann::json error = {{"success", false}, {"error", {{"code", "ERROR"}, {"message", message}}}};
    return Response().status(code).json(error);
}

http::response<http::string_body> Response::to_beast() const {
    http::response<http::string_body> res;
    res.result(status_code_);
    res.body() = body_;
    
    for (const auto& [key, value] : headers_) {
        res.set(key, value);
    }
    
    res.prepare_payload();
    return res;
}

} // namespace grotto::api
