#include <grotto/api/server.hpp>
#include <grotto/api/request.hpp>
#include <grotto/api/response.hpp>

#include <boost/asio.hpp>
#include <iostream>
#include <cassert>

using namespace grotto::api;
namespace asio = boost::asio;

void test_response_helpers() {
    // Test OK response
    auto ok = Response::ok({{"message", "success"}});
    assert(ok.status() == 200);
    
    // Test error response
    auto err = Response::bad_request("Invalid input");
    assert(err.status() == 400);
    
    // Test not found
    auto notf = Response::not_found();
    assert(notf.status() == 404);
    
    std::cout << "[test_response_helpers] PASSED\n";
}

void test_request_parsing() {
    // Test query parsing
    auto query = parse_query("key1=value1&key2=value2");
    assert(query.size() == 2);
    assert(query["key1"] == "value1");
    assert(query["key2"] == "value2");
    
    // Test empty query
    auto empty = parse_query("");
    assert(empty.empty());
    
    std::cout << "[test_request_parsing] PASSED\n";
}

void test_method_parsing() {
    assert(parse_method("GET") == HttpMethod::GET);
    assert(parse_method("POST") == HttpMethod::POST);
    assert(parse_method("PUT") == HttpMethod::PUT);
    assert(parse_method("DELETE") == HttpMethod::DELETE);
    assert(parse_method("invalid") == HttpMethod::UNKNOWN);
    
    std::cout << "[test_method_parsing] PASSED\n";
}

void test_api_key_manager() {
    ApiKeyManager manager;
    
    // Test empty
    assert(!manager.is_valid("any-key"));
    
    // Add key
    manager.add_key("test-key-123");
    assert(manager.is_valid("test-key-123"));
    assert(!manager.is_valid("wrong-key"));
    
    // Remove key
    manager.remove_key("test-key-123");
    assert(!manager.is_valid("test-key-123"));
    
    // Test key generation
    auto key = ApiKeyManager::generate_key();
    assert(key.size() > 0);
    assert(key.substr(0, 7) == "grotto_");
    
    std::cout << "[test_api_key_manager] PASSED\n";
}

void test_router() {
    Router router;
    
    bool get_called = false;
    bool post_called = false;
    
    router.get("/api/v1/test", [&](const Request&) -> Response {
        get_called = true;
        return Response::ok();
    });
    
    router.post("/api/v1/users/{id}", [&](const Request&) -> Response {
        post_called = true;
        return Response::created();
    });
    
    // Test GET match
    auto match1 = router.match(HttpMethod::GET, "/api/v1/test");
    assert(match1.found);
    match1.handler(Request{});
    assert(get_called);
    
    // Test POST with param
    auto match2 = router.match(HttpMethod::POST, "/api/v1/users/123");
    assert(match2.found);
    assert(match2.params["id"] == "123");
    match2.handler(Request{});
    assert(post_called);
    
    // Test no match
    auto match3 = router.match(HttpMethod::GET, "/not-found");
    assert(!match3.found);
    
    std::cout << "[test_router] PASSED\n";
}

int main() {
    std::cout << "Running grotto-server-api tests...\n\n";
    
    test_response_helpers();
    test_request_parsing();
    test_method_parsing();
    test_api_key_manager();
    test_router();
    
    std::cout << "\nAll tests PASSED!\n";
    return 0;
}
