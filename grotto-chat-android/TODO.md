# Grotto Android - TODO / Feature Parity Plan

> **Goal:** Bring the Android client to feature parity with the Desktop TUI client.

**Legend:** `[x]` done, `[~]` partial/stubbed, `[ ]` missing

---

## Feature Comparison Matrix

| Feature | Desktop | Android | Gap |
|---------|---------|---------|-----|
| Auth (challenge-response) | x | x | - |
| DM messaging (E2E) | x | x | - |
| Channel messaging (SKDM) | x | x | - |
| Ping/Pong keepalive | x | ? | Verify |
| MOTD display | x | x | Done (Tier 1) |
| Offline message delivery | x | ? | Verify |
| `/me` action messages | x | x | Done (Tier 1) |
| `/whois` | x | x | Done (Tier 1) |
| `/topic` | x | x | Done (Tier 1) |
| `/kick` `/ban` `/invite` | x | x | Done (Tier 1) |
| `/mode` | x | [ ] | Missing |
| `/nick` change | x | x | Done (Tier 1) |
| `/names` member list | x | x | Done (Tier 1) |
| `/search` message search | x | [ ] | Missing (TODO in code) |
| `/password` | x | x | Done (Tier 1) |
| Message retry | x | x | Done (Tier 1) |
| Message deletion | - | x | Done (Tier 1) |
| Markdown rendering | x | [ ] | Missing |
| Link preview (OG fetch) | x | [~] | URL detect works, no fetch |
| Typing indicators | [ ] | [ ] | Neither has it (proto needed) |
| Read receipts | [ ] | [ ] | Neither has it (proto needed) |
| Voice calls (1:1) | x | [~] | JNI stubs only |
| Voice rooms (multi) | x | [~] | JNI stubs only |
| PTT / VOX | x | [~] | UI exists, no audio I/O |
| File upload/download | [~] | [~] | Both stubbed, server ready |
| Unread count | x | x | Done (Tier 1) |
| Nick change notifications | x | x | Done (Tier 1) |
| Error message handling | x | x | Done (Tier 1) |
| Themes (multiple) | x (5) | x (2) | Android has dark/light only |
| Certificate pinning | x | x | - |
| Biometric auth | - | x | Android-only |
| Screen capture protect | - | x | Android-only |
| FCM push notifications | - | [~] | Partial, some TODOs |
| Safety numbers | x | x | - |

---

## Tier 1: Critical (Core Functionality Gaps)

### 1.1 Command System Expansion ✅
**Files:** `ChatViewModel.kt` (handleCommand), `GrottoConnectionManager.kt`

- [x] Command parser in `handleCommand()` — parses `/command args` from chat input
- [x] `/me <action>` — sends IrcCommand("me", rawArgs)
- [x] `/msg <user> <text>` — sends via `connectionManager.sendChat()`
- [x] `/whois <user>` — sends command to server
- [x] `/topic [text]` — get/set channel topic
- [x] `/names` — request member list
- [x] `/nick <new>` — sends nick change command
- [x] `/quit` — clean disconnect with optional reason
- [x] `/kick <user> [reason]` — op command
- [x] `/ban <user> [reason]` — op command
- [x] `/invite <user>` — invite to channel
- [x] `/password <old> <new>` — change password
- [x] Unknown commands forwarded to server as-is

### 1.2 Missing Message Type Handlers ✅
**Files:** `GrottoConnectionManager.kt` (dispatch section)

- [x] MT_NICK_CHANGE (62) — dispatched with `onNickChange` callback
- [x] MT_ERROR (99) — dispatched with `onServerError` callback
- [x] MT_MOTD (75) — dispatched with `onMotd` callback, proto added
- [x] MT_USER_INFO (63) — dispatched with `onUserInfo` callback
- [x] MT_COMMAND_RESPONSE (61) — already handled with `onCommandResponse`

### 1.3 Action Messages (`/me`) ✅
**Files:** `MessageBubble.kt`, `ChatViewModel.kt`

- [x] `/me` command in handleCommand sends IrcCommand("me", rawArgs)
- [x] MessageBubble renders ACTION type as italic `* nick content`
- [x] Entity→Domain mapping handles "action" → MessageType.ACTION

### 1.4 Message Retry ✅
**Files:** `ChatViewModel.kt`, `MessageDao.kt`, `MessageRepository.kt`

- [x] `retryMessage()` queries message by ID, sets SENDING, re-sends, updates status
- [x] `deleteMessage()` removes message from DB
- [x] DAO: `getMessageById()`, `deleteById()` added
- [x] UI: MessageBubble shows "Failed" + retry/delete icons, wired to ChatViewModel

### 1.5 Unread Count Tracking ✅
**Files:** `ChannelListViewModel.kt`, `MessageRepository.kt`, `MessageDao.kt`

- [x] DAO: `countUnread(channelId, afterTimestamp)` query
- [x] Repository: `countUnread()` method
- [x] ChannelListViewModel calculates unread per channel using `lastReadTs`
- [x] `markChannelAsRead()` already existed, updates `lastReadTs`
- [x] ChannelItem already shows badge with `unreadBadge` color when unreadCount > 0

---

## Tier 2: Important (Feature Parity)

### 2.1 Channel Member List ✅
**Files:** `MemberListDrawer.kt`, `UserActionsMenu.kt`, `ChannelMemberEntity.kt`

- [x] Send `/names` on channel join
- [x] Parse COMMAND_RESPONSE with member list (roles: @op, +voice, regular)
- [x] Member list drawer/sheet in chat screen
- [x] Show user roles with icons
- [x] Show online/offline status
- [x] Tap user → actions menu (DM, whois, kick/ban if op)
- [x] Update on join/part/nick_change events

### 2.2 Channel Topic ✅
**Files:** `ChatScreen.kt`, `ChannelEntity.kt`, `ChannelDao.kt`

- [x] Display topic in channel header/subtitle (ChatScreen TopAppBar shows topic)
- [x] `/topic` command to view/set (handled in ChatViewModel)
- [x] Topic change notifications (onCommandResponse handles topic updates)
- [x] Topic persisted to database (ChannelEntity.topic + updateTopic DAO method)

### 2.3 Markdown Rendering in Messages ✅
**Files:** `MessageBubble.kt`, `MarkdownText.kt`

- [x] Parse markdown in message text (custom parser in MarkdownText.kt)
- [x] Render **bold**, *italic*, `code`, ```code blocks```
- [x] Render links as clickable spans
- [x] Used in MessageBubble for all message content

### 2.4 Link Preview OG Fetching ✅
**Files:** `LinkPreviewRepository.kt`, `LinkPreviewCard.kt`, `SettingsScreen.kt`

- [x] Fetch Open Graph metadata (title, description, image) from URLs
- [x] Cache results in Room DB (link_previews table with 24h TTL)
- [x] Display in LinkPreviewCard component with domain extraction
- [x] Timeout and error handling (10s timeout, error state caching)
- [x] Respect user preference (enable/disable in settings)

### 2.5 Message Search ✅
**Files:** `ChatScreen.kt`, `MessageRepository.kt`, `MessageDao.kt`

- [x] Add search bar UI (toggle search button + OutlinedTextField in ChatScreen)
- [x] FTS query on Room DB messages (LIKE-based search in MessageDao)
- [x] Display search results with context (MessageBubble renders search results)
- [x] Real-time search as user types (onSearchQueryChanged in ChatViewModel)

### 2.6 Presence Updates in UI ✅
**Files:** `GrottoConnectionManager.kt`, `PeerIdentityEntity.kt`, `KeyRepository.kt`

- [x] Track all online users in database (presence_status in peer_identities table)
- [x] Handle MT_PRESENCE messages (GrottoConnectionManager.handlePresence)
- [x] Persist presence updates to Room DB (KeyRepository.updatePresence)
- [x] Query online users flow (PeerIdentityDao.getOnlineUsers)
- [ ] Show online/away/offline status in member list (needs member list UI)
- [ ] Show online indicator on DM contacts (needs DM UI)

---

## Tier 3: Voice & Calls (Major Feature)

### 3.1 Voice Engine Native Implementation
**Files:** `app/src/main/cpp/voice_engine.cpp`, `NativeVoice.kt`

- [ ] Oboe audio capture — microphone input → PCM frames
- [ ] Oboe audio playback — PCM frames → speaker output
- [ ] Opus codec integration — encode/decode audio frames
- [ ] libdatachannel WebRTC — peer connections, ICE/DTLS-SRTP
- [ ] JNI callbacks — audio level, peer join/leave, connection state
- [ ] Echo cancellation — AEC via Oboe or WebRTC
- [ ] Noise suppression — wire settings toggle to native

### 3.2 Voice Signaling
**Files:** `VoiceRepository.kt` (line ~140 TODO), `GrottoConnectionManager.kt`

- [ ] Send VoiceSignal (OFFER/ANSWER/ICE_CANDIDATE) via GrottoSocket
- [ ] Handle incoming VoiceSignal → pass to native engine
- [ ] Voice room join/leave → MT_VOICE_ROOM_JOIN/LEAVE
- [ ] Process MT_VOICE_ROOM_STATE → update participant list

### 3.3 1:1 Voice Calls
**Files:** `VoiceRepository.kt`, `CallScreen.kt`

- [ ] CALL_INVITE → send signal, show outgoing call UI
- [ ] CALL_ACCEPT → establish peer connection
- [ ] CALL_REJECT → clean up
- [ ] CALL_HANGUP → tear down
- [ ] Incoming call notification (full-screen intent exists)
- [ ] Call timer display

### 3.4 Voice Rooms
**Files:** `VoiceViewModel.kt`, voice UI components

- [ ] Join voice channel → send MT_VOICE_ROOM_JOIN
- [ ] Receive MT_VOICE_ROOM_STATE → connect to all participants
- [ ] Handle peer join/leave dynamically
- [ ] PTT mode (hold button to talk)
- [ ] VOX mode (voice activity detection with threshold)
- [ ] Display speaking indicators per participant
- [ ] Mute/deafen controls (already in UI, wire to native)

---

## Tier 4: File Transfer ✅

### 4.1 File Upload ✅
**Files:** `FileRepository.kt`, `GrottoConnectionManager.kt`

- [x] Implement `uploadFileInternal()` — chunk file, send MT_FILE_UPLOAD
- [x] Send FileUploadRequest with metadata
- [x] Stream FileUploadChunk messages (64KB chunks)
- [x] Progress tracking via callback
- [x] Respect 100MB limit
- [ ] SHA-256 checksum per chunk (server-side)
- [ ] Encrypt file data before upload (E2E) — future work

### 4.2 File Download ✅
**Files:** `FileRepository.kt`, `GrottoConnectionManager.kt`

- [x] Implement `downloadFileInternal()` — send MT_FILE_DOWNLOAD
- [x] Receive MT_FILE_CHUNK → write to storage
- [x] Handle MT_FILE_PROGRESS, MT_FILE_COMPLETE, MT_FILE_ERROR
- [x] Progress tracking via callback
- [ ] Resume support (start from chunk_index) — future work
- [ ] Verify checksum on completion — future work
- [ ] Decrypt after download — future work

### 4.3 File UI Integration ✅
**Files:** `FileAttachmentButton.kt`, `ChatScreen.kt`

- [x] File picker → upload flow (already in MessageInput)
- [x] Display file messages in chat (FileTransferProgress component)
- [x] Progress indicator during upload/download
- [x] Download handling via FileRepository callbacks
- [ ] Image preview for image files — future work
- [ ] Open file with system intent on completion — future work

---

## Tier 5: Polish & Nice-to-Have

### 5.1 FCM Push Notifications (finish TODOs)
**Files:** `FcmRepository.kt`, `GrottoMessagingService.kt`

- [ ] Send FCM token to server via MT_FCM_TOKEN protobuf (not just local store)
- [ ] Handle wakeup → trigger background sync
- [ ] Unregister token on logout via MT_FCM_UNREGISTER
- [ ] Grouped notifications for multiple messages
- [ ] Reply-from-notification action

### 5.2 Ping/Pong Keepalive
**Files:** `GrottoConnectionManager.kt`

- [ ] Verify MT_PING handler exists and responds with MT_PONG
- [ ] Add connection timeout detection (no pong → reconnect)
- [ ] Display connection quality indicator

### 5.3 Offline Message Delivery
**Files:** `GrottoConnectionManager.kt`

- [ ] Verify offline messages are received and processed on reconnect
- [ ] Show "catching up..." indicator during offline message sync
- [ ] Handle message ordering (timestamp-based)

### 5.4 Channel Creation (server-side)
**Files:** `ChannelListViewModel.kt` (line ~292 TODO)

- [ ] Send create channel command to server (not just local DB insert)
- [ ] Handle server validation/response

### 5.5 Additional Theme Options
**Files:** Theme system files

- [ ] Add Solarized theme
- [ ] Add Nord theme
- [ ] Add Dracula theme

### 5.6 Foreground Service
**Files:** `GrottoService.kt` (has TODOs)

- [ ] Implement persistent foreground service for connection
- [ ] Show notification with connection status
- [ ] Handle service lifecycle properly
- [ ] Battery optimization allowlist prompt

### 5.7 Safety Number Clipboard
**Files:** `SafetyNumberScreen.kt` (TODO in code)

- [ ] Implement copy-to-clipboard for safety numbers

---

## Implementation Order (Recommended)

1. **Tier 1** — basic IRC features that should "just work"
2. **Tier 2** — brings Android UX closer to desktop
3. **Tier 4** (files) before **Tier 3** (voice) — file transfer is simpler, server is ready
4. **Tier 3** — voice is the biggest undertaking (native C++ work)
5. **Tier 5** — polish items can be done alongside other tiers
