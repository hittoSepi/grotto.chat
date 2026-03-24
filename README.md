# Grotto

End-to-end encrypted chat and voice application for friend groups. Combines irssi's minimal terminal aesthetics with modern features like E2E encryption (Signal Protocol), voice rooms, file transfers, and link previews.

## Components

| Component | Language | Description |
|-----------|----------|-------------|
| [grotto-server](./grotto-server) | C++20 | TLS/TCP relay server with Signal Protocol key distribution |
| [grotto-installer](./grotto-installer) | C++20 | Linux server installer wizard (FTXUI) for prebuilt releases |
| [grotto-server-tui](./grotto-server-tui) | C++20 | Admin TUI for server management (FTXUI) |
| [grotto-server-api](./grotto-server-api) | C++20 | HTTP API library (Boost.Beast) |
| [grotto-client](./grotto-client) | C++20 | Desktop terminal client (FTXUI) with mouse support |
| [grotto-android](./grotto-android) | Kotlin/C++ | Android mobile client with Jetpack Compose |
| [grotto-plugin](./grotto-plugin) | C++/QuickJS | Plugin system for bots and extensions |
| [grotto-infra](./grotto-infra) | Docker/Node.js | Public directory service and landing page |

## Features

- **End-to-end encryption** via Signal Protocol (X3DH + Double Ratchet)
- **Group chats** with Sender Keys for efficient multi-party encryption
- **Voice rooms** and private calls (WebRTC P2P)
- **File transfers** with chunked upload/download
- **Mouse support** in desktop client (text selection, resizing, clicking)
- **Message search** with full-text search (FTS5) and filters
- **Admin TUI** for server management with log viewer, settings, user management
- **Public server directory** - discover Grotto servers
- **Themes** - Tokyo Night dark theme and clean light theme
- **Certificate pinning** with trust-on-first-use (TOFU)
- **Push notifications** via Firebase Cloud Messaging (Android)
- **Biometric authentication** for identity key access (Android)
- **Database encryption** via SQLCipher (Android)
- **Screen capture protection** (Android)

## Quick Start

### Server

```bash
cd grotto-server
mkdir build && cd build
cmake ..
cmake --build . --config Release
./grotto-server ../config/server.toml
```

Run headless (without TUI):
```bash
./grotto-server --headless ../config/server.toml
```

Check version:
```bash
./grotto-server --version
```

See [grotto-server/README.md](./grotto-server/README.md) for detailed build instructions.

Installer flow for Linux servers:

```bash
curl -fsSL https://chat.rausku.com/downloads/install.sh | sudo bash
```

### Server Admin TUI

```bash
cd grotto-server-tui
mkdir build && cd build
cmake ..
cmake --build . --config Release
./grotto-tui --socket \\.\pipe\grotto-admin   # Windows
./grotto-tui --socket /tmp/grotto-admin.sock   # Linux
```

The admin TUI connects to a running server via named pipe (Windows) or Unix socket (Linux).

Features: Log viewer (F1), Settings (F2), Bug Reports (F3), User sidebar with kick/ban/whois actions.

### Desktop Client

```bash
cd grotto-client
mkdir build && cd build
cmake ..
cmake --build . --config Release
./grotto-client
```

Quick connect via URL:
```bash
./grotto-client ircord://chat.example.com:6697
```

### Android Client

```bash
cd grotto-android
./gradlew assembleDebug
./gradlew installDebug
```

### Public Infrastructure

```bash
cd grotto-infra
./deploy.sh
```

## IRC Commands

| Command | Description |
|---------|-------------|
| `/join #channel` | Join a channel |
| `/part [#channel] [message]` | Leave a channel |
| `/msg <nick> <message>` | Private message |
| `/me <action>` | Action message |
| `/nick <new_nick>` | Change nickname |
| `/whois <nick>` | Show user info |
| `/password <old> <new>` | Change password |
| `/quit [message]` | Disconnect |
| `/kick <nick> [reason]` | Kick user (operator) |
| `/ban <nick> [reason]` | Ban user (operator) |
| `/invite <nick> [message]` | Invite user (operator) |
| `/topic <topic>` | Set channel topic (operator) |
| `/set <option> <value>` | Set channel option (operator) |

Channel options: `invite_only`, `moderated`, `topic_restricted`

## Security Model

- **Server sees**: IP addresses, who messages whom, timestamps
- **Server does NOT see**: Message content, file contents, voice audio (P2P)
- **Authentication**: Ed25519 identity key challenge-response
- **E2E Encryption**: Signal Protocol (X3DH initial + Double Ratchet for ongoing)
- **At-rest**: Identity keys encrypted with Argon2id + XChaCha20-Poly1305

## Architecture

**Client-server relay model** — the server never sees plaintext messages. The server acts as a relay and handles connection management, but all message content is encrypted end-to-end between clients.

```
                        TLS/TCP
 ┌─────────────┐   Protobuf frames   ┌─────────────┐
 │   Client A  │◄───────────────────►│   Server    │
 │  (Mobile/   │                     │  (Relay +   │
 │   Desktop)  │                     │  Key dist)  │
 └──────┬──────┘                     └──────┬──────┘
        │                                    │
        │    E2E Encrypted (Signal)          │
        └────────────────┬───────────────────┘
                         │
                ┌────────┴────────┐
                │    Client B     │
                │ (Mobile/Desktop)│
                └─────────────────┘

 ┌─────────────┐   Named pipe / Unix socket
 │  Admin TUI  │◄──────────────────►  Server
 │  (grotto-   │   JSON commands
 │   tui)      │
 └─────────────┘
```

## Wire Protocol

Length-prefixed framing over TCP/TLS:

```
┌──────────────┬────────────────────────┐
│  4 bytes     │  N bytes               │
│  (uint32 BE) │  Protobuf Envelope     │
└──────────────┴────────────────────────┘
```

Max message size: **64 KB**

Admin socket protocol: 4-byte BE length prefix + JSON (not Protobuf).

## Technology Stack

| Layer | Technology |
|-------|------------|
| Language | C++20 / Kotlin |
| Async I/O | Boost.Asio |
| Serialization | Protobuf |
| HTTP API | Boost.Beast |
| E2E Crypto | libsignal-protocol-c + libsodium |
| Database | SQLite (SQLiteCpp/Room) + SQLCipher |
| UI (Desktop) | FTXUI (terminal) |
| UI (Android) | Jetpack Compose |
| UI (Admin) | FTXUI (terminal) |
| Audio | miniaudio (desktop) / Oboe (Android) |
| Voice | WebRTC (libdatachannel + Opus) |
| Push | Firebase Cloud Messaging |
| Build | CMake + vcpkg / Gradle |

## Documentation

- [CHANGELOG.md](./CHANGELOG.md) - Project changelog
- [TODO.md](./TODO.md) - Planned features and roadmap
- [CLAUDE.md](./CLAUDE.md) - Claude Code context
- [UX-PLAN.md](./UX-PLAN.md) - UX development plan

## License

MIT License - see LICENSE file for details.

## Acknowledgments

- **irssi** - Inspiration for the terminal aesthetic
- **Signal** - End-to-end encryption protocol
- **Boost.Asio** - Async I/O library
- **FTXUI** - Terminal UI framework
