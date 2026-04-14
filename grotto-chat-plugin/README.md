# Grotto Plugin System

Extensible plugin system for Grotto supporting bots, client extensions, and server extensions. Plugins are written in JavaScript (ES2023) and run in a QuickJS sandbox.

**Stack:** C++20 · QuickJS · Boost.Asio · Protobuf

## Plugin Types

### 1. `bot` — Standalone Bot Client

Bots are headless Grotto clients with their own Ed25519 identity. They connect to the server like regular users and can:
- Join/leave channels
- Send/receive messages
- Register slash commands
- Make HTTP requests

**Use cases:**
- Slash command bots (`/roll`, `/poll`, `/remind`)
- Auto-responders (FAQ bot)
- Integration bots (weather, RSS, CI/CD notifications)

### 2. `client_extension` — Client Enhancement

Runs inside the user's desktop client process. No separate connection — uses the host client's NetClient.

**Use cases:**
- Local slash commands (`/clear`, `/theme`, `/export`)
- Message decoration (syntax highlighting, emoji replacement)
- Auto-complete extensions
- Custom notification rules

### 3. `server_extension` — Server Enhancement

Runs inside the server process. **Only sees metadata, never message content** (E2E encryption constraint).

**Use cases:**
- Automatic channel management
- Custom rate limiting
- Audit logging (joins, leaves, not messages)
- Auto-moderation (based on metadata only)

## Architecture

```
PluginManager
│
├── discover(plugins_dir)      # Scan and load plugins
├── load(plugin_dir)           # Load single plugin
├── unload(name)               # Unload plugin
├── dispatch(PluginEvent)      # Route events to plugins
└── dispatch_command(cmd)      # Route slash commands
│
├── PluginInstance (base)
│   ├── JSRuntime*             # Own QuickJS runtime (sandbox)
│   ├── PluginMeta             # Parsed plugin.json
│   ├── EventRegistry          # Event handlers
│   ├── CommandRegistry        # Command handlers
│   │
│   ├── BotPluginInstance      # Headless client bot
│   ├── ClientExtPlugin        # In-process client extension
│   └── ServerExtPlugin        # In-process server extension
```

## Plugin Package Structure

```
my-plugin/
├── plugin.json          # Required: metadata
├── main.js              # Required: entry point
├── config.toml          # Optional: plugin config
├── lib/                 # Optional: additional JS modules
│   └── utils.js
└── data/                # Optional: persistent data (runtime)
    └── state.json
```

### plugin.json

```json
{
  "name": "dicebot",
  "display_name": "Dice Bot",
  "version": "1.0.0",
  "type": "bot",
  "entry": "main.js",
  "description": "Rolls dice with /roll command",
  "min_host_version": "0.1.0",
  "bot": {
    "user_id": "DiceBot",
    "auto_join_channels": ["#general", "#games"]
  },
  "commands": [
    {
      "name": "/roll",
      "description": "Roll NdN dice",
      "usage": "/roll [NdN]"
    }
  ],
  "permissions": [
    "read_messages",
    "send_messages",
    "register_commands"
  ]
}
```

### Plugin Types

| Type | Value |
|------|-------|
| Bot | `"bot"` |
| Client Extension | `"client_extension"` |
| Server Extension | `"server_extension"` |

### Permissions

| Permission | Bot | Client | Server | Description |
|------------|-----|--------|--------|-------------|
| `read_messages` | ✓ | ✓ | ✗ | Read plaintext messages |
| `send_messages` | ✓ | ✓ | ✗ | Send messages |
| `register_commands` | ✓ | ✓ | ✗ | Register slash commands |
| `manage_channels` | ✗ | ✗ | ✓ | Create/delete channels |
| `manage_users` | ✗ | ✗ | ✓ | Kick/ban users |
| `http_request` | ✓ | ✓ | ✓ | HTTP requests to external APIs |
| `file_read` | ✓ | ✓ | ✓ | Read files in plugin directory |
| `file_write` | ✓ | ✓ | ✓ | Write to plugin data/ directory |

## JavaScript API

### Common API (All Plugin Types)

```javascript
// Events
grotto.on(eventName, callback)
// Events: "message", "user_joined", "user_left", "presence_changed", "shutdown"

// Commands
grotto.onCommand(name, options, callback)
// name: "/roll"
// options: { description: "Roll dice", usage: "/roll [NdN]" }
// callback: (ctx) => { ctx.sender, ctx.channel, ctx.args }

// Messages
grotto.sendMessage(channel, text)

// Configuration
grotto.getConfig()          // Returns plugin's config.toml as object
grotto.getPluginDir()       // Path to plugin directory
grotto.getDataDir()         // Path to plugin data/ directory

// Logging
grotto.log.info(msg)        // -> [PLUGIN:dicebot] msg
grotto.log.warn(msg)
grotto.log.error(msg)
grotto.log.debug(msg)

// Timing
// NOTE: Timer API is documented but not implemented yet in current runtime.
// Calls should be considered planned/experimental until js_timers.cpp is implemented.
grotto.setTimeout(callback, delay_ms)
grotto.setInterval(callback, interval_ms)
grotto.clearTimeout(id)
grotto.clearInterval(id)

// HTTP (requires http_request permission)
grotto.http.get(url, options)      // -> Promise<Response>
grotto.http.post(url, body, options)

// Files (restricted to plugin's data/ directory)
grotto.fs.readFile(relative_path)  // -> Promise<string>
grotto.fs.writeFile(relative_path, content)
```

### Bot-Specific API

```javascript
grotto.bot.userId                  // "DiceBot" - bot's own user_id
grotto.bot.joinChannel(channel)    // -> Promise<void>
grotto.bot.leaveChannel(channel)   // -> Promise<void>
grotto.bot.setPresence(status)     // "online" | "away"
```

### Client Extension API

```javascript
grotto.client.activeChannel()      // -> string
grotto.client.onlineUsers()        // -> string[]
grotto.client.channels()           // -> string[]
grotto.client.notify(text)         // Show notification in status bar
```

### Server Extension API

```javascript
grotto.server.kickUser(user_id, reason)
grotto.server.banUser(user_id, reason)
grotto.server.createChannel(channel_id)
grotto.server.deleteChannel(channel_id)
grotto.server.onlineUsers()        // -> string[]
grotto.server.channelMembers(channel_id)  // -> string[]
```

## Example: DiceBot

### plugin.json

```json
{
  "name": "dicebot",
  "display_name": "Dice Bot",
  "version": "1.0.0",
  "type": "bot",
  "entry": "main.js",
  "description": "Rolls dice with /roll command",
  "min_host_version": "0.1.0",
  "bot": {
    "user_id": "DiceBot",
    "auto_join_channels": ["#general"]
  },
  "commands": [
    {
      "name": "/roll",
      "description": "Roll NdN dice",
      "usage": "/roll [NdN]"
    }
  ],
  "permissions": ["read_messages", "send_messages", "register_commands"]
}
```

### main.js

```javascript
function parseDice(input) {
    if (!input) return { count: 1, sides: 6 };

    const match = input.match(/^(\d+)d(\d+)$/i);
    if (!match) {
        const n = parseInt(input);
        if (!isNaN(n) && n >= 1 && n <= 100) return { count: 1, sides: n };
        return null;
    }

    const count = parseInt(match[1]);
    const sides = parseInt(match[2]);

    if (count < 1 || count > 20 || sides < 2 || sides > 1000) return null;
    return { count, sides };
}

function rollDice(count, sides) {
    const rolls = [];
    for (let i = 0; i < count; i++) {
        rolls.push(Math.floor(Math.random() * sides) + 1);
    }
    return rolls;
}

// Register /roll command
grotto.onCommand("/roll", {
    description: "Roll NdN dice",
    usage: "/roll [NdN]"
}, (ctx) => {
    const dice = parseDice(ctx.args[0]);
    if (!dice) {
        grotto.sendMessage(ctx.channel,
            `${ctx.sender}: Invalid format. Usage: /roll 2d6`);
        return;
    }

    const rolls = rollDice(dice.count, dice.sides);
    const total = rolls.reduce((a, b) => a + b, 0);

    if (dice.count === 1) {
        grotto.sendMessage(ctx.channel,
            `🎲 ${ctx.sender}: d${dice.sides} → ${total}`);
    } else {
        grotto.sendMessage(ctx.channel,
            `🎲 ${ctx.sender}: ${dice.count}d${dice.sides} → ${rolls.join(" + ")} = ${total}`);
    }
});

// Log startup
grotto.log.info("DiceBot loaded!");
```

## Security & Sandboxing

Each plugin runs in its own QuickJS runtime with:

| Resource | Limit | Purpose |
|----------|-------|---------|
| JS heap | 16 MB | Prevent memory exhaustion |
| Stack | 1 MB | Prevent infinite recursion |
| Max errors/hour | 50 | Auto-unload broken plugins |
| HTTP timeout | 10 s | Prevent hanging requests |
| HTTP requests/min | 30 | Rate limit external APIs |
| File size | 1 MB | Limit plugin data files |

Errors are isolated — a plugin exception does not crash the host.

## Hot Reload

Plugins can be reloaded without restarting the host:

```bash
# Via host command (if supported)
/reload dicebot
```

During reload:
1. `grotto.onShutdown()` is called for cleanup
2. JS runtime is destroyed
3. For bots: connection stays open (no reconnect)
4. Plugin is re-parsed and re-initialized

## Installation Directories

### Server Plugins

```
grotto-server/
└── plugins/
    ├── channel-logger/
    └── auto-kick/
```

### Client Plugins

```
~/.config/grotto/
└── plugins/
    ├── dicebot/
    ├── welcomebot/
    └── syntax-highlight/
```

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler
- vcpkg with QuickJS

### Build

```bash
cd grotto-plugin
mkdir build && cd build
cmake ..
cmake --build .
```

## Related Projects

- [grotto-server](../grotto-server) — Server that can load server_extension plugins
- [grotto-client](../grotto-client) — Client that can load client_extension plugins
- [PLAN.md](PLAN.md) — Detailed technical design (Finnish)

## License

MIT License — see LICENSE file for details.
