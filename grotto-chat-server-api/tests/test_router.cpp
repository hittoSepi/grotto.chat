#include <grotto/api/router.hpp>
#include <grotto/api/response.hpp>
#include <iostream>
#include <cassert>

using namespace grotto::api;

void test_complex_routing() {
    Router router;
    
    // Register routes
    router.get("/api/v1/channels", [](const Request&) {
        return Response::ok({{"channels", {}}});
    });
    
    router.get("/api/v1/channels/{id}", [](const Request&) {
        return Response::ok({{"channel", {}}});
    });
    
    router.post("/api/v1/channels/{id}/messages", [](const Request&) {
        return Response::created();
    });
    
    // Test routes
    auto r1 = router.match(HttpMethod::GET, "/api/v1/channels");
    assert(r1.found);
    
    auto r2 = router.match(HttpMethod::GET, "/api/v1/channels/general");
    assert(r2.found);
    assert(r2.params["id"] == "general");
    
    auto r3 = router.match(HttpMethod::POST, "/api/v1/channels/general/messages");
    assert(r3.found);
    assert(r3.params["id"] == "general");
    
    // Wrong method
    auto r4 = router.match(HttpMethod::DELETE, "/api/v1/channels");
    assert(!r4.found);
    
    std::cout << "[test_complex_routing] PASSED\n";
}

void test_router_edge_cases() {
    Router router;
    
    router.get("/", [](const Request&) {
        return Response::ok();
    });
    
    router.get("/api/{version}/users/{id}/posts/{post_id}", [](const Request&) {
        return Response::ok();
    });
    
    // Root path
    auto r1 = router.match(HttpMethod::GET, "/");
    assert(r1.found);
    
    // Multiple params
    auto r2 = router.match(HttpMethod::GET, "/api/v1/users/123/posts/456");
    assert(r2.found);
    assert(r2.params["version"] == "v1");
    assert(r2.params["id"] == "123");
    assert(r2.params["post_id"] == "456");
    
    // No partial match
    auto r3 = router.match(HttpMethod::GET, "/api");
    assert(!r3.found);
    
    std::cout << "[test_router_edge_cases] PASSED\n";
}

int main() {
    std::cout << "Running router tests...\n\n";
    
    test_complex_routing();
    test_router_edge_cases();
    
    std::cout << "\nAll router tests PASSED!\n";
    return 0;
}
