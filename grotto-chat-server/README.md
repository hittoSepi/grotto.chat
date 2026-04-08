# Grotto Server

End-to-end encrypted chat and voice relay server. The server acts as a message relay and handles connection management, but never sees plaintext message content — all messages are E2E encrypted between clients using the Signal Protocol.

## Features

- 🔒 **TLS/TCP listener** with Boost.Asio
- 📡 **Message relay** between clients (E2E encrypted payload)
- 🔑 **Key distribution** — Signal Protocol pre-key bundles
- 👥 **Channel management** — create, join, leave channels
- 💾 **Offline message queue** with TTL (7 days)
- 🌐 **Public server directory** — opt-in public listing
- 📎 **File transfer relay** — chunked upload/download
- 🎙️ **Voice signaling** — WebRTC ICE relay
- ⏱️ **Rate limiting** — per-user, per-IP, per-command
- 🛡️ **IRC-style commands** — `/join`, `/part`, `/msg`, `/kick`, `/ban`

## Technology Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | C++20 | Performance, compatibility with client |
| Async I/O | Boost.Asio | Scalable event loop, cross-platform |
| Serialization | Protobuf | Schema evolution, good tooling |
| E2E Crypto | libsignal-protocol-c + libsodium | Signal Protocol (X3DH + Double Ratchet) |
| Database | SQLite (SQLiteCpp) | Embedded, minimal overhead |
| Config | toml11 | Simple TOML parsing |
| Logging | spdlog | Fast, structured logging |
| Testing | Catch2 | Header-only test framework |
| Build | CMake + vcpkg | Cross-platform, manifest-mode deps |

## Build Instructions

### Prerequisites

- **C++20 compatible compiler** (GCC 11+, Clang 13+, MSVC 2022+)
- **CMake 3.20+**
- **vcpkg** package manager
- **Git**

### Windows (Visual Studio 2022)

```bash
# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Build Grotto server
cd grotto-server
mkdir build && cd build
cmake ..
cmake --build . --config Release

# Run
.\Release\grotto-server.exe
```

### Linux (Debian/Ubuntu)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y cmake g++ git libssl-dev

# Install vcpkg (if not already installed)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh

# Build
cd grotto-server
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)

# Run
./grotto-server
```

## Configuration

Create a `server.toml` configuration file (see `config/server.toml.example`):

```toml
[server]
host = "0.0.0.0"
port = 6697
log_level = "info"
max_connections = 100

[tls]
cert_file = "certs/server.crt"
key_file = "certs/server.key"

[ping]
interval_sec = 60
timeout_sec = 120

# Public server listing (optional)
[directory]
enabled = true
public = true
server_name = "My Grotto Server"
server_description = "A friendly Grotto server"
url = "https://directory.grotto.dev"
ping_interval_sec = 300
```

### Optional: ClamAV on Ubuntu

If you want server-side virus scanning for file uploads, install the ClamAV daemon on Ubuntu and point Grotto at `clamd`.

Install packages:

```bash
sudo apt update
sudo apt install -y clamav clamav-daemon clamav-freshclam
```

Enable and start the update service plus the daemon:

```bash
sudo systemctl enable --now clamav-freshclam
sudo systemctl enable --now clamav-daemon
```

Check that the daemon is available:

```bash
sudo systemctl status clamav-freshclam
sudo systemctl status clamav-daemon
clamdscan --version
```

On Ubuntu, the simplest setup is usually the local Unix socket. Configure `server.toml` like this:

```toml
[antivirus]
clamav_socket = "/run/clamav/clamd.ctl"
clamav_host = "127.0.0.1"
clamav_port = 0
```

If your distro uses a different socket path, locate it with:

```bash
sudo find /run /var/run -name 'clamd*.ctl' 2>/dev/null
```

Important behavior:

- If ClamAV is not configured in `server.toml`, uploads continue normally without virus scanning.
- If ClamAV is configured but `clamd` is unavailable, uploads are rejected fail-closed.

### Optional: File Upload Policy

You can enforce upload size and MIME rules from `server.toml`:

```toml
[files]
max_upload_bytes = 104857600
max_total_storage_bytes = 0
max_user_storage_bytes = 0
allowed_mime_types = ["image/", "text/plain", "application/pdf"]
blocked_mime_types = ["application/x-msdownload", "application/x-dosexec"]
```

Rules:

- `max_upload_bytes`, `max_total_storage_bytes`, and `max_user_storage_bytes` are enforced by the server and advertised to clients after auth.
- `max_total_storage_bytes = 0` means the server-wide storage quota is disabled.
- `max_user_storage_bytes = 0` means per-user storage quota is disabled.
- Quotas reserve space at upload start, so incomplete uploads still count until they expire or are cleaned up.
- `allowed_mime_types = []` means "allow all unless blocked".
- `blocked_mime_types` always wins over allow rules.
- MIME prefix rules are supported with trailing `/` or `*`, for example `image/` and `audio/*`.

### Generate TLS Certificates

For development:

```bash
mkdir -p certs
openssl genrsa -out certs/server.key 2048
openssl req -new -x509 -key certs/server.key -out certs/server.crt -days 365
```

For production, use Let's Encrypt or a trusted CA.

## Running the Server

```bash
# From build directory
./grotto-server

# With custom config
./grotto-server /path/to/server.toml

# Check version
./grotto-server --version
```

The server will:
1. Load configuration from `server.toml`
2. Initialize TLS context with certificates
3. Start TCP listener on specified host:port
4. Accept client connections

## Project Structure

```
grotto-server/
├── CMakeLists.txt              # CMake build configuration
├── vcpkg.json                  # vcpkg dependencies
├── config/
│   └── server.toml.example     # Configuration template
├── scripts/
│   └── install.sh              # Server installation script
├── src/
│   ├── main.cpp                # Entry point
│   ├── server.cpp/hpp          # Server orchestration
│   ├── config.cpp/hpp          # Configuration loader
│   ├── server_config.hpp       # Default config template
│   ├── net/
│   │   ├── listener.cpp/hpp    # TLS/TCP acceptor
│   │   ├── session.cpp/hpp     # Per-connection state machine
│   │   ├── tls_context.cpp/hpp # SSL context factory
│   │   └── directory_client.cpp/hpp  # Public directory client
│   ├── proto/
│   │   └── grotto.proto        # Protobuf schema
│   └── ...
└── docs/
    ├── server-architecture.md
    └── client-architecture.md
```

## Wire Protocol

Length-prefixed framing over TCP/TLS:

```
┌──────────────┬────────────────────────┐
│  4 bytes     │  N bytes               │
│  (uint32 BE) │  Protobuf Envelope     │
└──────────────┴────────────────────────┘
```

Max message size: **64 KB** (enforced server-side).

## Security Model

- **Server sees**: IP addresses, who messages whom, timestamps
- **Server does NOT see**: message content, file contents, voice audio (P2P mode)
- **Auth**: Ed25519 identity key challenge-response (not password-based)
- **E2E**: Signal Protocol (X3DH initial + Double Ratchet for ongoing)
- **At-rest**: Identity keys encrypted with Argon2id + XChaCha20-Poly1305

## Public Server Directory

Servers can optionally register as public in the Grotto directory:

1. Set `directory.enabled = true` and `directory.public = true` in config
2. Server automatically registers with the directory service
3. Pings regularly to stay on the public list
4. Listed on the landing page at https://grotto.dev

## Installation Script

Quick server setup on Ubuntu/Debian with the stage-1 bootstrap:

```bash
curl -fsSL https://chat.rausku.com/downloads/install.sh | sudo bash
```

The bootstrap script only:

- Detects Linux architecture
- Downloads the matching `grotto-installer` binary
- Launches the installer wizard with `sudo` if needed

For local testing, build or download `grotto-installer` and run it directly:

```bash
./grotto-installer --manifest-url https://chat.rausku.com/downloads/installer-manifest.json
```

The installer will:
- Install runtime dependencies
- Download the matching prebuilt `grotto-server` artifact
- Configure SSL certificates (Let's Encrypt, self-signed, or existing certs)
- Set up systemd service
- Optionally configure firewall rules (UFW)

Legacy source-build installer remains available at:

```bash
sudo ./scripts/install-legacy-build.sh
```

## IRC Commands

| Command | Description |
|---------|-------------|
| `/join #channel` | Join a channel |
| `/part [#channel] [message]` | Leave channel |
| `/msg <user> <message>` | Send private message |
| `/nick <new_nick>` | Change nickname |
| `/whois <nick>` | Show user info |
| `/kick <nick> [reason]` | Kick user (operator only) |
| `/ban <nick> [reason]` | Ban user (operator only) |
| `/topic <text>` | Set channel topic (operator only) |
| `/invite <nick>` | Invite user to channel |
| `/quit [message]` | Disconnect from server |

## Current Implementation Status

### ✅ Implemented
- TLS/TCP listener with Boost.Asio
- Session management with strand serialization
- Protobuf wire protocol framing
- TOML configuration loading
- Thread pool architecture
- Message relay between clients
- Channel management
- Authentication system (Ed25519)
- Key distribution (pre-keys)
- Offline message queue
- Rate limiting
- File transfer relay
- Public server directory integration
- IRC-style commands

### 📋 Planned
- Voice signaling enhancements
- Link preview service

## Documentation

See the `docs/` directory:
- `server-architecture.md` - Server architecture (Finnish)
- `client-architecture.md` - Client architecture (Finnish)
- `grotto-tech-tradeoffs.md` - Technical decisions (Finnish)

## License

MIT License - see the LICENSE file for details.
