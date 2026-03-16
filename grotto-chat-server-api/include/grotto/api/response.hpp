#pragma once

#include "grotto/api/types.hpp"

#include <nlohmann/json.hpp>
#include <boost/beast.hpp>

namespace grotto::api {

namespace beast = boost::beast;
namespace http = beast::http;

using json = nlohmann::json;

// HTTP Response builder
class Response {
public:
    Response();
    
    // Status code
    Response& status(int code);
    int status() const { return status_code_; }
    
    // Headers
    Response& header(const std::string& key, const std::string& value);
    Response& content_type(const std::string& type);
    
    // Body
    Response& body(const std::string& body);
    Response& json(const json& data);
    
    // Success/Error helpers
    static Response ok(const json& data = {});
    static Response created(const json& data = {});
    static Response no_content();
    static Response bad_request(const std::string& message = "");
    static Response unauthorized(const std::string& message = "");
    static Response not_found(const std::string& message = "");
    static Response error(const std::string& message, int code = 500);
    
    // Convert to Beast response
    http::response<http::string_body> to_beast() const;
    
private:
    int status_code_ = 200;
    Headers headers_;
    std::string body_;
};

} // namespace grotto::api
