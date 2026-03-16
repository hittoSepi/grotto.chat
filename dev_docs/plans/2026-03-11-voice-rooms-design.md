# Voice Rooms Design

> **Status:** ✅ IMPLEMENTED (2026-03-11)

## Summary

Full mesh P2P voice rooms, channel-bound. Server handles signaling only. Push-to-talk default with optional voice activation. Max 8 participants per room.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Topology | Full mesh P2P | Simple, server stays lightweight (RPi), good for friend groups (<=8 users) |
| Room model | Channel-bound | Each text channel can have a voice session. No separate channel types. |
| Input mode | PTT default + VOX optional | PTT avoids background noise in TUI; VOX available via config |
| Max participants | 8 | Full mesh practical limit |

## Architecture

### Server

New `VoiceRoomManager` class tracks voice room membership:
- `std::unordered_map<string, std::set<string>> rooms_` (channel_id -> user_ids)
- `join(channel_id, user_id)` -> sends VoiceRoomState to joiner, broadcasts VoiceRoomJoin to others
- `leave(channel_id, user_id)` -> broadcasts VoiceRoomLeave to remaining
- `on_disconnect(user_id)` -> auto-leave all rooms

Existing `VoiceSignal` relay unchanged -- signals are always between specific user pairs, not broadcast.

### Proto additions

```protobuf
MT_VOICE_ROOM_JOIN  = 41;
MT_VOICE_ROOM_LEAVE = 42;
MT_VOICE_ROOM_STATE = 43;

message VoiceRoomJoin {
  string channel_id = 1;
  string user_id    = 2;
}

message VoiceRoomLeave {
  string channel_id = 1;
  string user_id    = 2;
}

message VoiceRoomState {
  string channel_id          = 1;
  repeated string participants = 2;
}
```

### Client flow

1. `/voice` -> send VoiceRoomJoin to server
2. Receive VoiceRoomState -> for each participant, create PeerConnection as offerer
3. Existing participants receive VoiceRoomJoin -> create PeerConnection to new user as answerer
4. SDP/ICE exchanged via existing VoiceSignal relay
5. `/voice leave` -> send VoiceRoomLeave, tear down all peer connections

### Controls

| Command | Action |
|---------|--------|
| `/voice` | Join voice in current channel |
| `/voice #channel` | Join voice in specific channel |
| `/voice leave` | Leave voice |
| `/mute` | Toggle mute |
| `/deafen` | Toggle deafen |
| `/ptt` | Toggle PTT/VOX mode |

PTT key: F1 (configurable in config.toml). VOX uses energy-based VAD with configurable threshold.

### Config

```toml
[voice]
ptt_key = "F1"
mode = "ptt"          # "ptt" or "vox"
vad_threshold = 0.02
input_device = ""
output_device = ""
```

### TUI display

Status bar voice indicator: `[Voice: #general 3 users] [PTT: F1]`

Channel user list prefixes: `@` = operator, `+` = voiced, voice participants get additional indicator.

Voice events shown as system messages in chat.

Speaking indicator: energy check on received audio frames, highlight active speaker.

### Error handling

- Not a channel member -> "Must join channel first"
- Room full (8) -> "Voice room is full"
- Already in voice -> auto-leave current before joining new
- Peer connection failure -> remove peer, warn in chat, keep other connections
- Disconnect -> server auto-removes, notifies others
