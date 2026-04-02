# Grotto Client

End-to-end encrypted chat and voice client for friend groups. Terminal UI with irssi-style aesthetics, mouse support, and full Signal Protocol encryption.

## Features

- 🔒 **End-to-end encryption** via Signal Protocol (X3DH + Double Ratchet)
- 👥 **Group chats** with Sender Keys for efficient multi-party encryption
- 🖱️ **Mouse support** — click channels, select text, resize panels
- 🎙️ **Voice rooms** — WebRTC P2P voice calls
- 📋 **Clipboard integration** — `Ctrl+V` paste support
- 🔗 **Link previews** — automatic URL fetching
- 🎨 **Themes** — Tokyo Night and other color schemes
- ⌨️ **IRC-style commands** — `/join`, `/part`, `/msg`, `/call`
- 🔐 **Certificate pinning** — trust-on-first-use (TOFU)

## Requirements

- Windows 10+ or Linux (x64)
- A server address and port from whoever runs your Grotto server

## First Run

1. Copy `client.toml.example` to `client.toml` (in the same folder as the executable, or see [Config Location](#config-location))
2. Edit `client.toml` and set the server address:
   ```toml
   [server]
   host = "your.server.address"
   port = 6697
   ```
3. Run `grotto-client.exe` (Windows) or `./grotto-client` (Linux)
4. If no username is set in the config, you will be prompted to enter one — it is saved automatically for future runs
5. Enter your passphrase when prompted (new users: choose a passphrase; returning users: enter the same one you used before)

> **Keep your passphrase safe.** It encrypts your identity key and is also used to re-bind your server-side identity if you reset local credentials.

## Quick Connect

Launch directly with an `ircord://` quick-connect URL:
```bash
grotto-client ircord://chat.example.com:6697
```

Or from a web browser when clicking an Grotto server link on the landing page.

## Config Location

If no `--config` flag is passed, the client looks for config in the platform default directory:

| Platform | Path |
|----------|------|
| Windows  | `%APPDATA%\grotto\client.toml` |
| Linux    | `~/.config/grotto/client.toml` |

You can also pass a custom path:
```bash
grotto-client --config /path/to/client.toml
```

## client.toml Reference

```toml
[server]
host = "your.server.address"   # Server hostname or IP
port = 6697                    # Server port
# cert_pin = ""                # SHA-256 certificate fingerprint (set automatically on first connect)

[identity]
user_id = "Alice"              # Your username (set automatically on first run if left blank)

[ui]
theme = "tokyo-night"          # UI color theme
timestamp_format = "%H:%M"     # Message timestamp format
max_messages = 1000            # Messages kept in memory per channel
show_user_list = true          # Show right-side user list panel
user_list_width = 20           # Width of user list in characters
user_list_collapsed = false    # Whether user list is collapsed

[voice]
input_device  = ""             # Microphone (empty = system default)
output_device = ""             # Speaker (empty = system default)
opus_bitrate  = 64000          # Voice quality in bits/s
frame_ms      = 20             # Audio frame size in milliseconds
jitter_buffer_frames = 4       # Buffered 20 ms frames before playback
ice_servers   = [              # Empty = built-in Google STUN fallback
  "stun:turn.example.com:3478",
  "turn:turn.example.com:3478?transport=udp",
  "turns:turn.example.com:5349?transport=tcp",
]
turn_username = "grotto"       # Optional TURN credential
turn_password = "secret"       # Optional TURN credential

[preview]
enabled       = true           # Enable link previews
fetch_timeout = 5              # Fetch timeout in seconds
max_cache     = 200            # Number of previews to cache
inline_images = true           # Try inline text thumbnails for direct image links
image_columns = 40             # Width for generated image thumbnails
image_rows    = 16             # Height for generated image thumbnails
terminal_graphics = "auto"     # "auto", "off", or "viewer-only" for kitty/iTerm2/sixel terminals

[tls]
verify_peer = true             # Verify server TLS certificate (keep true in production)
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Enter` | Send message |
| `Ctrl+V` | Paste clipboard text into the input line |
| `Tab` | Autocomplete username / channel |
| `PgUp` / `PgDn` | Scroll message history |
| Configured PTT key (`F1` by default) | Toggle PTT transmit on/off |
| `F2` | Toggle the right-side user list |
| `F12` | Open settings |
| `Alt+1..9` | Switch to channel by number |
| `Alt+Left` / `Alt+Right` | Cycle channels |
| `/join #channel` | Join a channel |
| `/vmode` | Toggle voice mode (`PTT` / `VOX`) |
| `/part` | Leave current channel |
| `/msg <user> <text>` | Send a private message |
| `/call <user>` | Start a voice call |
| `/hangup` | End current call |
| `/quit` | Exit the client |

## Mouse Support

| Action | Description |
|--------|-------------|
| Click channel tab | Switch active channel |
| Click user list item | Mention user in input |
| Right-click user | Context menu (if available) |
| Drag user list border | Resize panel width |
| Click + drag message | Select text |
| Double-click message | Select entire message |
| Triple-click message | Select message with sender, auto-copies to clipboard |
| Mouse wheel | Scroll message history |

## Command Line Options

```
grotto-client [OPTIONS] [ircord://host:port]

Options:
  --config <path>   Path to client.toml (default: platform config dir)
  --user   <id>     Override username from config
  --clear-creds     Clear remembered credentials and local encrypted identity
  --help            Show this help
```

## Security

- **End-to-end encrypted** — the server never sees message content
- **Signal Protocol** — X3DH key agreement + Double Ratchet for forward secrecy
- **Identity key** — stored locally, encrypted at rest with your passphrase
- **Voice** — WebRTC P2P (audio does not pass through the server)
- **Certificate pinning** — automatic trust-on-first-use for server certificates

## Building from Source

If you want the full clean-machine setup for Arch Linux, Ubuntu, or Windows, use the detailed guide:

- [HOW-TO-BUILD.md](./HOW-TO-BUILD.md)

### Quick Start

Requirements:

- **CMake** 3.20+
- **Git**
- **C++20 compiler**: MSVC 2022+ on Windows, GCC 11+ or Clang 13+ on Linux
- **vcpkg** with `VCPKG_ROOT` pointing to your local vcpkg checkout

The project uses:

- `vcpkg.json` for packaged dependencies
- `FetchContent` for some upstream libraries during configure/build

### Windows

Run these in `Developer PowerShell for VS 2022` or another shell where MSVC is available:

```powershell
git clone https://github.com/hittoSepi/grotto.chat.git
cd grotto.chat\grotto-chat-client
cmake -S . -B build `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release --target grotto-client
```

Output:

- `build\Release\grotto-client.exe`
- `build\Release\client.toml`
- `build\Release\help\`
- `build\Release\resources\`

### Linux

```bash
git clone https://github.com/hittoSepi/grotto.chat.git
cd grotto.chat/grotto-chat-client
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

Output:

- `build/grotto-client`
- `build/client.toml`
- `build/help/`
- `build/resources/`

### Running multiple instances on the same machine

Each instance needs its own config directory to avoid sharing identity keys and SQLite databases. Use `--config`:

```bash
grotto-client --config ~/.config/grotto-alice/client.toml
grotto-client --config ~/.config/grotto-bob/client.toml
```

## Voice / ICE Setup

If `[voice].ice_servers` is empty, the client falls back to public Google STUN servers. That is acceptable for quick testing, but it is not reliable enough for real-world NAT traversal.

For a server you run yourself, configure one STUN entry and at least one TURN entry:

```toml
[voice]
ice_servers = [
  "stun:turn.example.com:3478",
  "turn:turn.example.com:3478?transport=udp",
  "turns:turn.example.com:5349?transport=tcp",
]
turn_username = "grotto"
turn_password = "replace-with-your-turn-password"
```

When `ice_servers` is defined, the client uses only that list and does not append the built-in Google STUN fallback. If `turn_username` is set, the same TURN credentials are applied to all configured ICE servers.

## Troubleshooting

**"Crypto init failed (wrong passphrase?)"**
You entered the wrong passphrase for your local identity. Try again, or use `--clear-creds` / `CLEAR CREDS` and then log back in with the same username and passphrase to re-sync keys on the server.

**Connection refused / timeout**
Check that `host` and `port` in `client.toml` match the server, and that the server is running.

**TLS errors with `verify_peer = true`**
If the server uses a self-signed certificate, set `verify_peer = false` in `[tls]` (only do this on a trusted network).

**Voice not working**
Make sure `input_device` and `output_device` are empty (system default) or set to a valid device name. Check that the server is reachable for signaling.

## Related Projects

- [grotto-server](../grotto-server) — C++ relay server
- [grotto-android](../grotto-android) — Android mobile client
- [grotto-plugin](../grotto-plugin) — Plugin system

## License

MIT License — see LICENSE file for details.
