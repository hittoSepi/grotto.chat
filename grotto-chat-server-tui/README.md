# grotto-server-tui

FTXUI-based admin terminal UI for [grotto-server](https://github.com/hittoSepi/grotto-server). Built as a static library with a standalone executable that can attach/detach to a running server.

## Features

- **Real-time log viewer** (F1) — color-coded by level, scrollable
- **Settings editor** (F2) — hot-reload server config without restart
- **Bug report viewer** (F3) — manage reports from the HTTP API
- **User sidebar** — online users & channels, right-click context menu (Kick/Ban/Whois)
- **Command line** — `/kick`, `/ban`, `/whois` commands
- **Attach/detach** — connect to a running headless server at any time

## Layout

```
┌─────────────────────────────────────────────────────────────┐
│  [F1 Loki] [F2 Asetukset] [F3 Bug Reports]        v0.1.0  │
├───────────────────────────────────────┬─────────────────────┤
│                                       │  Käyttäjät (3)      │
│  [INFO] User "Sepi" connected         │                     │
│  [INFO] #general: 2 members           │  ● Sepi             │
│  [WARN] Rate limit: Jansen            │  ● Jansen           │
│  [DEBUG] Ping timeout check...        │  ● Mikko            │
│                                       │                     │
│                                       │  Kanavat (2)        │
│                                       │  # general          │
│                                       │  # dev              │
├───────────────────────────────────────┴─────────────────────┤
│ > /kick Jansen spammaus                                     │
└─────────────────────────────────────────────────────────────┘
```

## Usage

```bash
# Option 1: Integrated — server starts with TUI
grotto-server

# Option 2: Headless — attach TUI later
grotto-server --headless
grotto-tui                      # attach
                                # Ctrl+D to detach
grotto-tui                      # re-attach any time

# Connect to custom socket path
grotto-tui --socket /path/to/socket
```

## Building

### Standalone

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### As part of grotto-server

```cmake
# In grotto-server/CMakeLists.txt
option(GROTTO_CHAT_SERVER_TUI "Build with TUI support" ON)
if(GROTTO_CHAT_SERVER_TUI)
    add_subdirectory(../grotto-server-tui ${CMAKE_BINARY_DIR}/grotto-server-tui)
    target_link_libraries(grotto-server PRIVATE grotto-server-tui)
    target_compile_definitions(grotto-server PRIVATE GROTTO_CHAT_HAS_TUI)
endif()
```

Disable TUI for headless builds (e.g. RPi): `cmake -DGROTTO_CHAT_SERVER_TUI=OFF ..`

## Dependencies

| Dependency | Source | Purpose |
|-----------|--------|---------|
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) v5.0.0 | FetchContent | Terminal UI |
| [nlohmann/json](https://github.com/nlohmann/json) | vcpkg | Admin protocol |
| [Boost.Asio](https://www.boost.org/) | vcpkg | Socket communication |
| [spdlog](https://github.com/gabime/spdlog) | vcpkg | Logging |

## Architecture

```
grotto-server                         grotto-tui
┌──────────────┐                     ┌─────────────────┐
│ Server Core  │                     │  FTXUI UI        │
│ (Asio pool)  │                     │                  │
│              │    admin socket     │  LogView         │
│ AdminSocket  │◄──────────────────►│  UserView        │
│ Listener     │  JSON protocol      │  SettingsView    │
│              │  (named pipe /      │  BugReportView   │
│ spdlog sink  │   unix socket)      │                  │
└──────────────┘                     └─────────────────┘
```

Admin protocol uses 4-byte big-endian length prefix + JSON messages over a local socket (named pipe on Windows, Unix domain socket on Linux).

## License

Same as [grotto-server](https://github.com/hittoSepi/grotto-server).
