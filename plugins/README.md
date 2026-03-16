# Grotto Plugins

This directory contains Grotto plugins.

## Plugin Types

### 1. Bot Plugins (`type: "bot"`)

Standalone bot clients with their own identity. Connect to server like regular users.

### 2. Client Extensions (`type: "client_extension"`)

Run inside user's desktop client. Can register local commands and access UI.

### 3. Server Extensions (`type: "server_extension"`)

Run inside server process. Handle server-side logic and storage.

---

## Available Plugins

### Bug Reporter - Hybrid (Client + Server)

**Architecture:** Client extension captures `/bug` command → sends to server → server extension stores it.

This is better than a bot because:
- No separate bot connection needed
- Centralized storage on server
- Works even if bot is offline
- Cleaner separation of concerns

**Components:**

#### Client Extension: `bug-reporter-client/`
- Captures `/bug <description>` command
- Validates input
- Sends to server via `grotto.client.sendBugReport()`
- Shows local notification on success/failure

**Permissions:** `register_commands`

#### Server Extension: `bug-reporter-server/`
- Listens for bug reports via `grotto.server.onBugReport()`
- Stores bugs in `data/bugs.json`
- Manages bug ID counter

**Permissions:** `file_write`

**Files:**
```
plugins/
├── bug-reporter-client/
│   ├── plugin.json      # type: client_extension
│   └── main.js
└── bug-reporter-server/
    ├── plugin.json      # type: server_extension
    ├── main.js
    └── data/
        └── bugs.json    # Stored reports (runtime)
```

**Installation:**
```bash
# Client side (user config)
cp -r plugins/bug-reporter-client ~/.config/grotto/plugins/

# Server side
sudo cp -r plugins/bug-reporter-server /etc/grotto-server/plugins/
```

---

### Bug Reporter - Bot Version (Legacy)

**Plugin:** `bug-reporter/`

Simple bot-based version. Requires bot to be online and in channel.

**Commands:**
- `/bug <description>` - Submit bug report
- `/bugs [count]` - List recent reports

---

## New Plugin APIs

### Client Extension

```javascript
// Send bug report to server
grotto.client.sendBugReport(description)
```

### Server Extension

```javascript
// Register handler for incoming bug reports
grotto.server.onBugReport((userId, description) => {
    // Handle bug report
});
```

### File System (All types)

```javascript
// Read/write files in plugin's data/ directory
grotto.fs.readFile("bugs.json")     // Returns string
grotto.fs.writeFile("bugs.json", content)
```

Files are sandboxed to `plugin/data/` for security.

---

## Plugin System Updates

Added to `grotto-plugin`:

1. **`grotto.fs` API** - File I/O with path sandboxing
2. **`grotto.client.sendBugReport()`** - Client→server communication
3. **`grotto.server.onBugReport()`** - Server-side handler registration
4. **`PluginContext` callbacks** - `send_bug_report`, `on_bug_report`
