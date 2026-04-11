# Changelog

All notable changes and achievements for the Grotto project.

## 2026-04-11 - Desktop Client Local Voice Self-Test

### Implemented
- **Desktop client**: Added `/voicetest` to open a local microphone loopback path without creating a WebRTC session
- **Voice engine**: Reused the existing capture, noise suppression, limiter, and playback chain for local audio monitoring
- **Status bar**: Added a dedicated mic-test voice indicator so local monitoring is visible without fake RTC peer stats
- **Docs**: Updated client help, README command list, TODOs, and changelog entries for the new voice workflow

## 2026-03-15 - Server TUI: Main Layout Wired (Task 9)

### Implemented
- **AdminTui main layout**: Full FTXUI layout with tab bar (F1 Loki, F2 Asetukset, F3 Bug Reports),
  left content pane, right user/channel sidebar, status bar, and command input
- **Event dispatch**: Socket events routed to LogView, UserView, SettingsView, BugReportView
- **Command input**: `/kick`, `/ban`, `/whois` commands parsed and sent via AdminSocketClient
- **User actions**: Context menu actions (kick/ban/whois) from UserView wired to server commands
- **Config changes**: SettingsView changes sent as `set_config` commands
- **Bug report actions**: Status updates sent as `update_bug_report` commands
- **CLI**: `--socket <path>` and `--help` flags for the standalone `grotto-tui` executable

## 2026-03-13 - Mouse Support for Desktop Client

### Implemented (UX Plan D6)
- **D6.1 Button and UI Interaction**
  - Click channel tabs to switch active channel
  - Click user list items to mention users in input
  - Right-click user list items for context actions
  - Toggle user list panel via UI button

- **D6.2 Subwindow Resizing**
  - Drag user list panel divider to resize width
  - Panel width persisted to configuration
  - Visual feedback during resize operation

- **D6.3 Text Selection and Interactions**
  - Click and drag to select text in message area
  - Double-click to select entire message
  - Triple-click to select message with sender, auto-copies to clipboard
  - Mouse wheel to scroll message history (3 lines per tick)
  - Clipboard integration (Windows/macOS/Linux)

### Technical Details
- New `MouseTracker` class for mouse state management
- `UIRegion` struct for hit testing UI components
- `MouseConfig` constants for timing and thresholds
- Cross-platform clipboard support via native APIs

### Limitations (Terminal UI Constraints)
- Character-level text selection not possible (message-level only)
- True cursor shape changes not supported by FTXUI
- Link clicking depends on terminal emulator support
- No true popup context menus (simulated via input injection)

## 2026-03-13 - Channel Messaging Fixed

### Fixed
- **Server**: Channel messages now broadcast to all members instead of being stored offline
- **Server**: MT_COMMAND (60) message type now handled correctly for IRC commands
- **Server**: KeyBundle now includes `recipient_for` field for proper DM session establishment
- **Server**: Added version tracking (git commit + build timestamp) to verify deployment
- **Android**: Group encryption now sends Sender Key Distribution Message (SKDM) with first message
- **Android**: Fixed JNI bridge to return both ciphertext and SKDM for group encryption
- **Desktop**: Group encryption/decryption working with proper SKDM handling

### Result
- ✅ Channel messages work bidirectionally between Android and Desktop clients
- ✅ Server displays version info on startup for deployment verification
- ✅ Users can join channels with `/join #channel` and send/receive encrypted messages

## 2026-03-12 - Server Fixes

### Fixed
- **Server**: Fixed protobuf compilation issue with `has_recipient_for()` method
- **Server**: Updated CMakeLists.txt to generate version header automatically
- **Server**: Added build timestamp and git commit hash to startup logs

## 2026-03-11 - Initial E2E Encryption

### Implemented
- Signal Protocol integration for end-to-end encryption
- X3DH key agreement for DMs
- Sender Keys for group/channel encryption
- One-time pre-keys (OPKs) for offline messaging
- Signed pre-keys (SPKs) for session establishment

### Platforms
- Android: Native C++ crypto via JNI
- Desktop: C++ client with libsignal-protocol-c
- Server: Key distribution (key bundles)

## Technical Achievements

### Encryption
- [x] E2E encryption for direct messages (1:1)
- [x] E2E encryption for group channels (Sender Keys)
- [x] Forward secrecy via ephemeral keys
- [x] Key verification via safety numbers

### Server Infrastructure
- [x] TLS/TCP socket server
- [x] Protocol Buffers wire format
- [x] SQLite database for user/key storage
- [x] Offline message queue with TTL
- [x] Rate limiting (per-user, per-IP, per-command)
- [x] IRC-style command handling

### Client Features
- [x] Real-time chat with encryption
- [x] Channel/room support
- [x] Voice rooms (WebRTC foundation)
- [x] File transfer (chunked, virus scanning)
- [x] Presence indicators (online/away/offline)

### Security
- [x] Certificate pinning (TOFU model)
- [x] Database encryption (SQLCipher)
- [x] Biometric authentication (Android)
- [x] Screen capture protection (FLAG_SECURE)
