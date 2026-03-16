# Grotto Server TUI - Design Document

**Date:** 2026-03-15
**Status:** Approved

---

## Overview

TUI (Terminal User Interface) for grotto-server admin management. Built as a separate static library (`grotto-server-tui/`) with an standalone executable (`grotto-tui`). Uses FTXUI, same as the desktop client.

## Goals

- Real-time log viewing with color-coded levels
- User management (kick, ban) via command line and context menus
- Hot-reload settings without server restart
- Bug report viewing (from HTTP API)
- Attach/detach TUI to running headless server

---

## 1. Architecture

```
                    grotto-server
  ┌──────────────┐    ┌─────────────────────┐
  │  Server Core  │    │ AdminSocketListener │
  │  (Asio pool)  │<-->│ (local socket)      │
  └──────────────┘    └────────┬────────────┘
                               │ JSON protocol
                               │ (named pipe / unix socket)
                    ┌──────────▼──────────┐
                    │ AdminSocketClient   │
                    └──────────┬──────────┘
                    ┌──────────▼──────────┐
                    │   FTXUI TUI         │
                    └─────────────────────┘
                        grotto-tui
```

### Two usage modes

```bash
# Option 1: Integrated - server + TUI in same process
$ grotto-server

# Option 2: Separate - attach TUI to running server
$ grotto-server --headless
$ grotto-tui                    # attach, Ctrl+D = detach
$ grotto-tui                    # re-attach any time
```

Server always starts `AdminSocketListener`. In integrated mode, it spawns
`grotto-tui` logic in-process connecting to the same local socket.

---

## 2. Project Structure

```
grotto-server-tui/
├── CMakeLists.txt                  # static lib + executable
├── include/grotto/tui/
│   ├── admin_tui.hpp               # Main class: starts FTXUI loop
│   ├── admin_protocol.hpp          # JSON message serialization
│   ├── admin_socket_client.hpp     # Connects to admin socket
│   └── admin_socket_listener.hpp   # Server side: listens on socket
├── src/
│   ├── admin_tui.cpp               # Layout, tabs, event handling
│   ├── admin_protocol.cpp          # Protocol encode/decode
│   ├── admin_socket_client.cpp     # Socket connection to server
│   ├── admin_socket_listener.cpp   # Server accepts TUI connections
│   ├── log_view.cpp                # Log view component
│   ├── user_view.cpp               # User list + context menu
│   ├── settings_view.cpp           # Hot-reload settings
│   └── bugreport_view.cpp         # Bug report list
├── src/main.cpp                    # grotto-tui executable entry
└── tests/
```

---

## 3. Admin Protocol

4-byte big-endian length prefix + JSON payload (same framing as Grotto wire protocol, but JSON instead of protobuf).

### Server -> TUI (events)

```json
{"type":"log","level":"info","msg":"User Sepi connected","ts":"2026-03-15T12:00:00Z"}
{"type":"users","data":[{"id":"Sepi","ip":"1.2.3.4","connected":"2026-03-15T11:50:00Z"}]}
{"type":"channels","data":[{"name":"#general","members":2}]}
{"type":"stats","uptime":3600,"connections":3,"version":"abc123"}
{"type":"bug_reports","data":[{"id":1,"user":"Sepi","desc":"...","status":"new"}]}
{"type":"config","data":{"log_level":"info","msg_rate_per_sec":20}}
```

### TUI -> Server (commands)

```json
{"cmd":"kick","user_id":"Jansen","reason":"spammaus"}
{"cmd":"ban","user_id":"Jansen","reason":"spam"}
{"cmd":"set_config","key":"log_level","value":"debug"}
{"cmd":"update_bug_report","id":1,"status":"resolved"}
{"cmd":"subscribe","events":["log","users","channels","stats"]}
```

---

## 4. Layout

```
┌─────────────────────────────────────────────────────────────┐
│  [F1 Loki] [F2 Asetukset] [F3 Bug Reports]        v0.1.0  │
├───────────────────────────────────────┬─────────────────────┤
│                                       │  Kayttajat (3)      │
│  [INFO] User "Sepi" connected         │                     │
│  [INFO] #general: 2 members           │  * Sepi             │
│  [WARN] Rate limit: Jansen            │  * Jansen           │
│  [DEBUG] Ping timeout check...        │  * Mikko            │
│  [INFO] User "Mikko" joined #dev      │                     │
│                                       │  Kanavat (2)        │
│                                       │  # general          │
│                                       │  # dev              │
│                                       │                     │
├───────────────────────────────────────┴─────────────────────┤
│ > /kick Jansen spammaus                                     │
└─────────────────────────────────────────────────────────────┘
```

- **70/30 split** - log on left, sidebar on right (always visible)
- **Tabs F1-F3** switch the left pane view
- **Command line** at bottom, always available
- **Mouse:** right-click on user -> context menu (Kick, Ban, Whois)
- **Status bar:** version, uptime, connection count

---

## 5. Views

### F1 Log View
Scrollable log list, color-coded by level:
- DEBUG = gray
- INFO = white
- WARN = yellow
- ERROR = red

Server streams log lines in real-time over the admin socket.

### F2 Settings View
Editable list of hot-reload settings. Enter to edit value.

**Hot-reload (immediate):**
- `log_level` (debug/info/warn/error)
- `msg_rate_per_sec`
- `commands_per_min`
- `joins_per_min`
- `max_connections`
- `abuse_threshold`
- `ban_duration_min`

**Read-only (require restart):**
- TLS certs, port, bind address, db path

### F3 Bug Reports View
List from HTTP API. Status (new/read/resolved), description, sender, timestamp.
Enter opens details, status change via context menu.

---

## 6. Threading & Socket Path

### Server side
- `AdminSocketListener` in Boost.Asio io_context (same thread pool)
- Windows: named pipe `\\.\pipe\grotto-admin`
- Linux: Unix socket `/tmp/grotto-admin.sock` or `$XDG_RUNTIME_DIR/grotto-admin.sock`
- One TUI connection at a time

### TUI side
- `AdminSocketClient` connects to socket, reads events async
- FTXUI `ScreenInteractive::Fullscreen()` in its own thread
- Socket events -> `PostEvent()` into TUI thread

### Integrated mode
- Server starts `AdminSocketListener`
- Same process launches TUI logic connecting to localhost
- Result identical, no separate process needed

---

## 7. CMake Integration

```cmake
# grotto-server-tui/CMakeLists.txt
add_library(grotto-server-tui STATIC ...)
add_executable(grotto-tui src/main.cpp)
target_link_libraries(grotto-tui PRIVATE grotto-server-tui)

# grotto-server/CMakeLists.txt
option(GROTTO_CHAT_SERVER_TUI "Build with TUI support" ON)
if(GROTTO_CHAT_SERVER_TUI)
    add_subdirectory(../grotto-server-tui grotto-server-tui)
    target_link_libraries(grotto-server PRIVATE grotto-server-tui)
endif()
```

RPi headless build: `cmake -DGROTTO_CHAT_SERVER_TUI=OFF ..` - no FTXUI dependency.

---

## 8. Dependencies

| Dependency | Purpose |
|-----------|---------|
| FTXUI | Terminal UI framework |
| nlohmann/json | Admin protocol serialization |
| Boost.Asio | Socket communication (from server) |
| spdlog | Logging (from server) |
