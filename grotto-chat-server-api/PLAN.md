# Grotto HTTP API Server — PLAN

HTTP API server library for grotto-server. Provides REST API for external integrations.

**Stack:** C++20 · Boost.Beast · Boost.Asio · JSON

---

## 1. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    grotto-server                             │
│                                                              │
│  ┌─────────────────────┐    ┌──────────────────────────┐   │
│  │   Grotto Protocol   │    │    HTTP API Server       │   │
│  │   (TLS+Protobuf)    │    │    (Boost.Beast)         │   │
│  │                     │    │                          │   │
│  │  - Chat clients     │    │  - External integrations │   │
│  │  - Voice            │    │  - Web dashboards        │   │
│  │  - File transfers   │    │  - Bot webhooks          │   │
│  └─────────────────────┘    └──────────────────────────┘   │
│           │                           │                     │
│           └───────────┬───────────────┘                     │
│                       │                                     │
│              ┌────────▼────────┐                            │
│              │  Server Core    │                            │
│              │  - Channels     │                            │
│              │  - Users        │                            │
│              │  - Messages     │                            │
│              └─────────────────┘                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. API Design

### 2.1 Endpoints

| Method | Endpoint | Description | Auth |
|--------|----------|-------------|------|
| GET | `/health` | Health check | No |
| GET | `/api/v1/channels` | List channels | API Key |
| GET | `/api/v1/channels/{id}` | Channel info | API Key |
| POST | `/api/v1/channels/{id}/messages` | Send message | API Key |
| GET | `/api/v1/channels/{id}/messages` | Get messages | API Key |
| GET | `/api/v1/users` | List users | API Key |
| GET | `/api/v1/users/{id}` | User info | API Key |
| POST | `/api/v1/bug-reports` | Submit bug report | No* |
| GET | `/api/v1/bug-reports` | List bug reports | API Key |
| POST | `/api/v1/webhooks/{id}` | Webhook receiver | Webhook secret |

*Bug reports can be rate-limited without auth

### 2.2 Response Format

```json
{
  "success": true,
  "data": { ... },
  "error": null
}
```

Error response:
```json
{
  "success": false,
  "data": null,
  "error": {
    "code": "INVALID_API_KEY",
    "message": "The provided API key is invalid"
  }
}
```

---

## 3. C++ API

### 3.1 Usage

```cpp
#include <grotto/api/server.hpp>

// Create server
grotto::api::Server server(io_context, port);

// Register handlers
server.get("/health", [](const Request& req) -> Response {
    return Response::json({{"status", "ok"}});
});

server.post("/api/v1/bug-reports", [](const Request& req) -> Response {
    auto body = req.json();
    std::string description = body["description"];
    
    // Store bug report
    store_bug_report(description);
    
    return Response::json({{"id", bug_id}}).status(201);
});

// Start listening
server.start();
```

### 3.2 Class Hierarchy

```
Server
├── listen(port)           → Bind and start accepting
├── stop()                 → Graceful shutdown
├── get(path, handler)     → Register GET handler
├── post(path, handler)    → Register POST handler
├── put(path, handler)     → Register PUT handler
├── del(path, handler)     → Register DELETE handler
├── use(middleware)        → Add middleware
│
├── Router                 → Path routing
│   ├── add_route(method, path, handler)
│   └── match(method, path) → handler
│
├── MiddlewareChain
│   └── process(request)  → modified request
│
└── ConnectionManager
    ├── accept()
    └── close_all()

Request
├── method()       → GET, POST, etc.
├── path()         → /api/v1/channels
├── headers()      → map<string, string>
├── query()        → map<string, string>
├── body()         → string
├── json()         → parsed JSON
└── remote_addr()  → IP:port

Response
├── status(code)   → Set status code
├── header(k, v)   → Add header
├── body(text)     → Set body
├── json(obj)      → Set JSON body
└── send()         → Finalize
```

---

## 4. Security

### 4.1 Authentication

**API Key:**
```
Authorization: Bearer <api-key>
```

**Webhook Secret:**
```
X-Webhook-Secret: <secret>
```

### 4.2 Rate Limiting

| Endpoint | Limit |
|----------|-------|
| `/health` | 100/min |
| `/api/v1/*` | 60/min |
| `/api/v1/bug-reports` | 10/min |

### 4.3 CORS

```http
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE
Access-Control-Allow-Headers: Authorization, Content-Type
```

---

## 5. Configuration

### 5.1 Server Config (TOML)

```toml
[http_api]
enabled = true
port = 8080
bind_address = "0.0.0.0"
api_keys = ["dev-key-change-in-production"]
cors_enabled = true
rate_limit_enabled = true

# Webhook endpoints
[http_api.webhooks]
enabled = true
secret = "webhook-secret-here"
```

### 5.2 Installation Wizard

```
? Enable HTTP API server? (Y/n) 
? HTTP API port: (8080) 
? Require API key authentication? (Y/n) 
? Generate random API key? (Y/n) 
  API Key: grotto_xxxxxxxxxxxx
```

---

## 6. Integration with grotto-server

### 6.1 CMake Integration

```cmake
# In grotto-server/CMakeLists.txt
add_subdirectory(grotto-server-api)

target_link_libraries(grotto-server PRIVATE
    grotto-server-api
)
```

### 6.2 Server Initialization

```cpp
// In server.cpp
void Server::start() {
    // ... existing Grotto protocol listener
    
    // Start HTTP API if enabled
    if (config_.http_api_enabled) {
        api_server_ = std::make_unique<api::Server>(
            io_context_, 
            config_.http_api_port
        );
        
        setup_api_routes();
        api_server_->start();
    }
}
```

### 6.3 Bug Report Integration

The HTTP API enables the client/server bug reporter plugins to communicate:

```
Client Extension                    Server Extension
     │                                    │
     │  POST /api/v1/bug-reports          │
     │ ──────────────────────────────►    │
     │  {                                 │
     │    "user_id": "Sepi",              │
     │    "description": "..."            │
     │  }                                 │
     │                                    │
     │         201 Created                │
     │ ◄────────────────────────────────  │
     │         { "id": 42 }               │
```

---

## 7. File Structure

```
grotto-server-api/
├── CMakeLists.txt
├── PLAN.md
├── README.md
├── LICENSE
├── include/grotto/api/
│   ├── server.hpp
│   ├── request.hpp
│   ├── response.hpp
│   ├── router.hpp
│   ├── middleware.hpp
│   └── auth.hpp
├── src/
│   ├── server.cpp
│   ├── router.cpp
│   ├── middleware.cpp
│   └── auth.cpp
└── tests/
    ├── test_server.cpp
    └── test_router.cpp
```

---

## 8. Dependencies

- **Boost.Beast** - HTTP server
- **Boost.Asio** - Networking
- **nlohmann/json** - JSON parsing
- **spdlog** - Logging

---

## 9. Future Extensions

- WebSocket support for real-time events
- GraphQL endpoint
- gRPC gateway
- OpenAPI/Swagger documentation
