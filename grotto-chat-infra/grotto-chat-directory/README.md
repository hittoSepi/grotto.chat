# Grotto Directory Service

A simple Node.js service that maintains a public list of Grotto servers.

## Features

- **Server Registration**: Grotto servers register themselves with the directory
- **Heartbeat Pings**: Servers send periodic pings to stay listed (10 min timeout)
- **REST API**: Simple JSON API for server registration and listing
- **CORS Enabled**: Landing page can fetch server list from any domain

## Quick Start

```bash
# Install dependencies
npm install

# Start the server
npm start

# Or for development with auto-reload
npm run dev
```

The service will start on port 3000 by default (or use `PORT` environment variable).

## API Endpoints

### Register a Server
```bash
POST /api/servers/register
Content-Type: application/json

{
  "host": "chat.example.com",
  "port": 6697,
  "name": "My Grotto Server",
  "description": "A friendly place to chat"
}

Response:
{
  "server_id": "uuid-generated-by-directory",
  "message": "Server registered successfully"
}
```

### Ping (Keep Alive)
```bash
POST /api/servers/ping
Content-Type: application/json

{
  "server_id": "uuid-from-registration"
}

Response:
{
  "message": "Ping received",
  "server_id": "uuid"
}
```

### List All Servers
```bash
GET /api/servers

Response:
{
  "count": 2,
  "servers": [
    {
      "id": "uuid",
      "host": "chat.example.com",
      "port": 6697,
      "name": "My Grotto Server",
      "description": "...",
      "online": true,
      "lastPing": "2026-03-14T10:30:00.000Z"
    }
  ]
}
```

### Unregister
```bash
POST /api/servers/unregister
Content-Type: application/json

{
  "server_id": "uuid"
}
```

## Configuration

The Grotto server can be configured to register with a directory service:

```toml
# server.toml
[server]
public = true  # Mark server as public

[directory]
enabled = true
url = "https://directory.grotto.dev"
ping_interval_sec = 300  # Ping every 5 minutes
server_name = "My Awesome Server"
description = "A friendly Grotto server"
```

## Production Deployment

For production use, consider:

1. **Database**: Replace in-memory storage with Redis or PostgreSQL
2. **Rate Limiting**: Add rate limits to prevent abuse
3. **Authentication**: Add API keys for server registration
4. **SSL**: Use HTTPS in production
5. **Monitoring**: Add health checks and metrics

## Environment Variables

- `PORT`: Server port (default: 3000)

## Architecture

```
┌─────────────────┐     Register/Ping     ┌─────────────────┐
│  Grotto Server  │ ────────────────────▶ │  Directory API  │
│  (C++ / Boost)  │                       │   (Node.js)     │
└─────────────────┘                       └────────┬────────┘
                                                   │
                            GET /api/servers       │
                                                   ▼
                                           ┌─────────────────┐
                                           │  Landing Page   │
                                           │   (Browser)     │
                                           └─────────────────┘
```
