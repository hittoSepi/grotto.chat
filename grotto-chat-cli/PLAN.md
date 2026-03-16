# grotto CLI — Implementation Plan

## Goal

Create an `grotto` CLI binary that manages a running Grotto server via the admin socket. Hybrid command system: builtin C++ commands + extensible QuickJS script commands.

## Architecture

### Command System (Hybrid)

```
CommandRegistry
  +-- ICommand (interface)
  |     +-- name(), description(), usage()
  |     +-- type(): fire_and_forget | streaming | interactive
  |     +-- execute(CommandContext&): int
  |
  +-- Builtin commands (C++, compiled)
  |     ShutdownCommand, RestartCommand, StatusCommand,
  |     KickCommand, BanCommand, UsersCommand,
  |     LogtailCommand, TuiCommand, UpdateCommand,
  |     HelpCommand, VersionCommand
  |
  +-- ScriptCommand (QuickJS, loaded at runtime)
        Scans ~/.grotto/commands/*.js
        Each script exports { command, execute }
```

### CommandContext

Shared context passed to every command:

```cpp
struct CommandContext {
    AdminSocketClient& admin;      // admin socket connection
    std::vector<std::string> args; // positional args after command name
    std::map<std::string, std::string> flags; // --key value flags

    void print(const std::string& msg);      // stdout
    void error(const std::string& msg);      // stderr
    nlohmann::json send_and_wait(const nlohmann::json& cmd); // send + wait for response
    void subscribe(const std::string& event_type,
                   std::function<void(const nlohmann::json&)> cb); // streaming
};
```

### Command Types

| Type | Behavior |
|------|----------|
| `fire_and_forget` | Connect, send command, print response, exit |
| `streaming` | Connect, subscribe to events, print until Ctrl+C |
| `interactive` | Launch TUI (delegates to existing AdminTui) |

### Protocol Extensions (Server-side)

New admin commands to add to `Server::handle_admin_command()`:

| Command | Action |
|---------|--------|
| `shutdown` | Graceful shutdown (stop accepting, drain connections, exit) |
| `restart` | Shutdown + re-exec (or signal parent process) |
| `update` | Server-side: git pull + rebuild (or delegate to script) |
| `status` | Return { uptime, version, connections, memory } |
| `users` | Return full user list (already available via periodic state) |
| `logtail` | Subscribe client to real-time log events |

Response format:
```json
{ "type": "response", "cmd": "status", "ok": true, "data": { ... } }
{ "type": "error", "cmd": "shutdown", "ok": false, "error": "permission denied" }
```

### Script Engine Integration

Reuse QuickJS from `grotto-plugin/`. Provide JS APIs:

```js
// Global APIs
grotto.exec(program, args)  // spawn process, return { code, stdout, stderr }
grotto.fs.read(path)        // read file
grotto.fs.write(path, data) // write file
grotto.fs.exists(path)      // check existence

// Context APIs (passed to execute())
ctx.print(msg)              // print to terminal
ctx.error(msg)              // print to stderr
ctx.args                    // string[] of positional args
ctx.flags                   // object of --key=value flags
ctx.admin.send(cmd, params) // send admin command, returns response
ctx.admin.subscribe(type, callback) // subscribe to event stream
```

## Project Structure

```
grotto/
  CMakeLists.txt
  README.md
  PLAN.md
  src/
    main.cpp                    # Entry point, arg parsing, dispatch
    command_registry.hpp/cpp    # Command registration and lookup
    command_context.hpp/cpp     # Shared context for command execution
    command.hpp                 # ICommand interface
    commands/                   # One file per builtin command
      shutdown.cpp
      restart.cpp
      status.cpp
      kick.cpp
      ban.cpp
      users.cpp
      logtail.cpp
      tui.cpp
      update.cpp
      help.cpp
      version.cpp
    script/
      script_engine.hpp/cpp     # QuickJS integration
      script_command.hpp/cpp    # ICommand wrapper for JS scripts
```

## Implementation Phases

### Phase 1: Core CLI Framework
1. CMakeLists.txt — link grotto-server-tui (AdminSocketClient), nlohmann/json
2. ICommand interface + CommandRegistry
3. CommandContext with admin socket connection
4. main.cpp — arg parsing, registry lookup, dispatch
5. HelpCommand + VersionCommand (local, no socket needed)

### Phase 2: Builtin Commands
6. StatusCommand — connect, send `status`, print response
7. UsersCommand — send `users`, print table
8. ShutdownCommand — send `shutdown`
9. RestartCommand — send `restart`
10. KickCommand / BanCommand — with args
11. LogtailCommand — streaming (subscribe to logs, print until Ctrl+C)
12. TuiCommand — launch AdminTui
13. UpdateCommand — send `update` or run locally

### Phase 3: Server-side Command Handlers
14. Add `shutdown`, `restart`, `status`, `users`, `logtail` to server's `handle_admin_command()`
15. Add response protocol (ok/error JSON responses)
16. Add `subscribe` support for log streaming to non-TUI clients

### Phase 4: Script Commands
17. Integrate QuickJS from grotto-plugin
18. ScriptCommand wrapper (loads .js, calls execute())
19. JS API bindings (ctx.admin, grotto.exec, grotto.fs)
20. Auto-scan ~/.grotto/commands/ on startup
21. `grotto help` lists both builtin and script commands

### Phase 5: Polish
22. Error handling (server not running, connection refused)
23. Signal handling (Ctrl+C for streaming commands)
24. Colored output (optional, detect terminal)
25. Tab completion hints in help output
26. Config file (~/.grotto/config.json) for socket path override

## Dependencies

| Dependency | Source | Purpose |
|------------|--------|---------|
| grotto-server-tui | ../grotto-server-tui | AdminSocketClient, protocol |
| grotto-plugin | ../grotto-plugin | QuickJS engine |
| nlohmann/json | FetchContent | JSON |
| Boost.Asio | System/FetchContent | Async I/O |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Produces: build/grotto(.exe)
```

## Notes

- Admin socket path: `\\.\pipe\grotto-admin` (Win) / `$XDG_RUNTIME_DIR/grotto-admin.sock` (Linux)
- Max message size: 64KB (matches existing protocol)
- CLI exit codes: 0 = success, 1 = command error, 2 = connection error
- Script commands discovered at startup, not hot-reloaded
