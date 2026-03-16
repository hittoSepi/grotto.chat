# grotto CLI

Command-line management tool for Grotto server. Controls a running server via the admin socket.

## Features

- **Fire-and-forget commands** — status, users, shutdown, restart, kick, ban, update
- **Streaming commands** — logtail for real-time log viewing
- **Interactive mode** — launch TUI for full admin interface
- **Extensible** — Phase 4 will add QuickJS script commands

## Usage

```bash
# Show help
grotto help

# Show server status
grotto status

# List online users
grotto users

# Kick/ban users
grotto kick <username> [reason]
grotto ban <username> [reason]

# Stream logs in real-time
grotto logtail
grotto logtail --level error    # filter by level

# Shut down or restart server
grotto shutdown
grotto restart

# Update server (server-side)
grotto update

# Launch interactive TUI
grotto tui

# Custom socket path
grotto --socket /path/to/socket status
```

## Building

### Windows

```powershell
cmake -S grotto -B build/grotto `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="grotto-client/vcpkg_installed/x64-windows/share"
  
cmake --build build/grotto --config Release
```

### Linux

```bash
cmake -S grotto -B build/grotto -DCMAKE_BUILD_TYPE=Release
cmake --build build/grotto
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Command error (unknown command, invalid args, server error) |
| 2 | Connection error (server not running) |

## Admin Socket

The CLI connects to the server's admin socket:

- **Windows**: `\\.\pipe\grotto-admin`
- **Linux**: `$XDG_RUNTIME_DIR/grotto-admin.sock` or `/tmp/grotto-admin.sock`

Use `--socket <path>` to override.

## Architecture

```
┌──────────────┐      admin socket      ┌──────────────┐
│  grotto CLI  │  ◄──────────────────►  │ grotto-server │
│              │    JSON protocol       │               │
│  - status    │   (named pipe /        │  AdminSocket  │
│  - kick/ban  │    unix socket)        │  Listener     │
│  - logtail   │                        │               │
│  - tui       │                        │  handle_admin │
└──────────────┘                        └──────────────┘
```

## Dependencies

| Dependency | Source | Purpose |
|------------|--------|---------|
| grotto-server-tui | `../grotto-server-tui` | AdminSocketClient, protocol |
| nlohmann/json | vcpkg / FetchContent | JSON |
| Boost.Asio | vcpkg | Async I/O |
| spdlog | vcpkg | Logging (via grotto-server-tui) |

## License

Same as [grotto-server](https://github.com/hittoSepi/grotto-server).
