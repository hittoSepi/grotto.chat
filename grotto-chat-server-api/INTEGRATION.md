# Grotto HTTP API Integration Guide

How to integrate `grotto-server-api` with `grotto-server`.

## 1. Add as Submodule

```bash
cd grotto-server
git submodule add https://github.com/your-org/grotto-server-api.git
```

## 2. Update CMakeLists.txt

```cmake
# Add subdirectory
add_subdirectory(grotto-server-api)

# Link to server executable
target_link_libraries(grotto-server PRIVATE
    # ... existing libs ...
    grotto-server-api
)
```

## 3. Configuration

Add to `config/server.toml`:

```toml
[http_api]
enabled = true
port = 8080
bind_address = "0.0.0.0"
api_keys = ["change-me-in-production"]
cors_enabled = true
rate_limit_enabled = true
rate_limit_requests = 60
```

## 4. Server Integration

```cpp
// src/server.hpp
#include <grotto/api/server.hpp>

class Server {
    // ... existing members ...
    std::unique_ptr<grotto::api::Server> api_server_;
    
    void setup_api_routes();
};
```

```cpp
// src/server.cpp
void Server::start() {
    // ... start Grotto protocol listener ...
    
    // Start HTTP API
    if (config_.http_api.enabled) {
        grotto::api::ServerConfig api_config;
        api_config.port = config_.http_api.port;
        api_config.bind_address = config_.http_api.bind_address;
        api_config.api_keys = config_.http_api.api_keys;
        api_config.cors_enabled = config_.http_api.cors_enabled;
        api_config.rate_limit_enabled = config_.http_api.rate_limit_enabled;
        
        api_server_ = std::make_unique<grotto::api::Server>(
            io_context_, api_config);
        
        setup_api_routes();
        api_server_->start();
    }
}

void Server::setup_api_routes() {
    // Health check
    api_server_->get("/health", [](const auto& req) {
        return grotto::api::Response::ok({
            {"status", "ok"},
            {"service", "grotto-server"}
        });
    });
    
    // Channels API
    api_server_->get("/api/v1/channels", [this](const auto& req) {
        auto channels = channel_manager_->list();
        return grotto::api::Response::ok({{"channels", channels}});
    });
    
    // Bug reports (plugin integration)
    api_server_->post("/api/v1/bug-reports", [this](const auto& req) {
        auto body = req.json_body();
        std::string desc = body["description"];
        std::string user = body.value("user_id", "anonymous");
        
        // Store bug report...
        int id = store_bug_report(user, desc);
        
        return grotto::api::Response::created({{"id", id}});
    });
}
```

## 5. Installation Wizard

Add to setup wizard:

```cpp
void SetupWizard::run() {
    // ... existing questions ...
    
    bool enable_api = ask_yes_no("Enable HTTP API server?", true);
    
    if (enable_api) {
        int port = ask_int("HTTP API port:", 8080);
        bool require_auth = ask_yes_no("Require API key authentication?", true);
        
        std::string api_key;
        if (require_auth) {
            if (ask_yes_no("Generate random API key?", true)) {
                api_key = grotto::api::ApiKeyManager::generate_key();
                std::cout << "Generated API key: " << api_key << "\n";
                std::cout << "Save this key!\n";
            } else {
                api_key = ask_string("Enter API key:");
            }
        }
        
        // Save to config
        config.http_api.enabled = true;
        config.http_api.port = port;
        config.http_api.api_keys = {api_key};
    }
}
```

## 6. Plugin Integration

### Client Extension → Server Extension via HTTP

```javascript
// Client extension
grotto.onCommand("/bug", {}, (ctx) => {
    const description = ctx.args.join(" ");
    
    // Send via HTTP API
    fetch("http://localhost:8080/api/v1/bug-reports", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({
            user_id: ctx.sender,
            description: description
        })
    }).then(() => {
        grotto.client.notify("Bug report sent!");
    });
});
```

## 7. Security Checklist

- [ ] Change default API key in production
- [ ] Enable rate limiting
- [ ] Use HTTPS (reverse proxy)
- [ ] Restrict bind_address if needed
- [ ] Firewall rules for API port
- [ ] Monitor API access logs

## 8. Testing

```bash
# Health check
curl http://localhost:8080/health

# Submit bug report (no auth)
curl -X POST http://localhost:8080/api/v1/bug-reports \
  -H "Content-Type: application/json" \
  -d '{"description": "Test bug"}'

# List reports (with auth)
curl http://localhost:8080/api/v1/bug-reports \
  -H "Authorization: Bearer your-api-key"
```
