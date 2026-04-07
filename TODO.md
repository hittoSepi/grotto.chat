## TODOS & IDEAS

### Desktop + Server Readiness
- [~] **Server file-security hardening**
  - [x] ClamAV client implementation for configured TCP / Unix-socket deployments
  - [x] Fail closed when antivirus is configured but unavailable or scan fails
  - [x] Validate optional upload chunk SHA-256 checksums
  - [x] Include final file checksum in `MT_FILE_COMPLETE`
- [~] **Desktop file-transfer integrity**
  - [x] Send SHA-256 checksum per upload chunk
  - [x] Verify final checksum on transfer completion
  - [x] Resolve named audio-device binding in the miniaudio backend with default-device fallback

### Client File UX
- [~] **Desktop file-transfer UX**
  - [x] Treat terminal drag-and-drop as pasted local file path and prepare `/upload`
  - [x] Improve `/upload` and `/download` feedback in the client
  - [ ] Add `/transfers` command for active and recent transfers
  - [ ] Add visible upload/download progress UI
  - [ ] Surface server-side quota failures clearly in the client

### Documentation
- [ ] **Translate Finnish documents to English**
  - `grotto-android/docs/android/architecture.md`
  - `grotto-android/docs/client-architecture.md`
  - `grotto-android/docs/server-architecture.md`
  - `grotto-android/docs/grotto-tech-tradeoffs.md`
  - `grotto-android/docs/grotto-design.md`

### Channel Operators & Moderation
- [x] **Channel operator system**
  - [x] First user in channel becomes operator (@nick)
  - [x] Operator commands:
    - `/kick <nick> [reason]`
    - `/ban <nick> [reason]`
    - `/invite <nick> [message]`
    - `/topic <channel_topic>`/
    - `/set <option> <value>`
  - [x] Channel options:
    - `invite_only <true|false>`
    - `moderated <true|false>` (only voiced/op can speak)
    - `topic_restricted <true|false>` (only op can change topic)
  - [x] User modes: operator (@), voiced (+)

### IRC Commands
- [x] **General commands** (Server-side)
  - [x] `/whois <nick>` - Show user info
  - [x] `/nick <new_nick>` - Change nickname
  - [x] `/password <old> <new>` - Change password
  - [x] `/join <#channel>` - Join channel
  - [x] `/part [#channel] [message]` - Leave channel
  - [x] `/quit [message]` - Disconnect from server
  - [x] `/msg <nick> <message>` - Private message
  - [x] `/me <action>` - Action message (/me eats pizza)

### Android Native Features
- [~] **VoiceEngine JNI implementation** (foundation complete)
  - [x] `NativeVoice.kt` interface
  - [x] `voice_engine.cpp` - libdatachannel + Opus + Oboe (foundation)
  - [~] Audio capture/playback callbacks (stub)
  - [~] ICE/DTLS-SRTP handling (stub)
- [x] **Push notifications (FCM)**
  - [x] Firebase Cloud Messaging integration
  - [x] Wakeup notifications for offline messages
  - [x] Notification service
  - [x] Notification settings UI
- [x] **Database encryption (SQLCipher)**
  - [x] Encrypt Room database (256-bit key)
  - [x] Android Keystore key storage
  - [x] Biometric auth for identity key (optional)

### Server Features
- [x] **Public server directory**
  - [x] Server can register as public
  - [x] Directory service (Node.js)
  - [x] Landing page server list
  - [x] ircord:// protocol handler
  - [x] Web client fallback
- [x] **File transfer relay**
  - [x] File upload/download endpoints
  - [x] Chunked transfer
  - [x] Progress tracking
- [x] **Offline message queue**
  - [x] Store messages for offline users (DM + channel)
  - [x] Deliver on reconnect
  - [x] TTL expiration (7 days)
  - [x] Queue size limits (100/user, 100k total)
- [x] **Rate limiting**
  - [x] Per-user message rate limits (20/sec)
  - [x] Command rate limits (30/min)
  - [x] Channel join rate limits (5/min)
  - [x] Connection rate limiting (10/min per IP)
  - [x] Auto-ban on repeated abuse (5 violations = 30min ban)

### Security Enhancements
- [x] **Certificate pinning**
  - [x] Pin server certificate in OkHttp
  - [x] Certificate rotation handling (backup pins)
  - [x] UI for managing pins
  - [x] Trust on first use (TOFU) support
- [x] **Screen capture protection**
  - [x] FLAG_SECURE for chat screen
  - [x] Configurable in settings

### Voice & Calls
- [x] **Voice rooms**
  - [x] Join/leave voice channels
  - [x] Push-to-talk or voice activation
  - [x] Participant list with speaking indicators
  - [x] Mute/deafen controls
- [x] **Private calls**
  - [x] 1:1 WebRTC calls
  - [x] Call invite/accept/reject
  - [ ] Call quality indicators

### Polish
- [x] **Voice polish**
  - [x] Status bar voice indicator with PTT/VOX mode
  - [x] Voice events as system messages (join/leave notifications)
  - [x] Speaking indicators (energy-based highlighting)
- [ ] **Link preview improvements**
  - [ ] Image previews
  - [ ] Video embeds
  - [ ] Open Graph metadata
- [x] **Message search**
  - [x] Full-text search in channels (FTS5)
  - [x] Search by user/date (filters ready)
- [ ] **Typing indicators**
  - [ ] Show "X is typing..."
- [ ] **Read receipts**
  - [ ] Show message delivery status
  - [ ] Show read status for DMs

## IN PROGRESS
- [~] VoiceEngine JNI implementation (foundation complete, needs libdatachannel + Opus + Oboe)

## COMPLETED
- [x] Android Signal Protocol crypto (C++ NDK)
- [x] Dark/Light theme system
- [x] **Channel messaging (E2E encrypted)** - 2026-03-13
  - [x] Bidirectional messaging between Android and Desktop
  - [x] Sender Key distribution for group encryption
  - [x] Server broadcast logic for channels

## COMPLETED
- [x] Basic TLS/TCP connection
- [x] Protobuf wire protocol
- [x] Basic chat UI
- [x] Channel list
- [x] Settings screen
