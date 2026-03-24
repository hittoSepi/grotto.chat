# Voice Rooms Implementation Plan

> **Status:** ✅ COMPLETED (2026-03-11)
>
> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add full-mesh P2P voice rooms to Grotto, channel-bound, with PTT/VOX input modes.

**Architecture:** Server tracks voice room membership and relays join/leave notifications. Clients establish direct WebRTC peer connections (full mesh) for audio. Existing VoiceEngine handles P2P signaling and audio — this plan adds room coordination on top.

**Tech Stack:** C++20, Protobuf, Boost.Asio (server), libdatachannel + Opus + miniaudio (client, already wired), FTXUI (TUI)

---

### Task 1: Proto — Add Voice Room Messages

**Files:**
- Modify: `grotto-server/src/proto/grotto.proto:34-37` (add message types after MT_VOICE_SIGNAL)
- Modify: `grotto-server/src/proto/grotto.proto:146` (add messages after VoiceSignal)
- Sync: `grotto-client/src/proto/grotto.proto` (exact same changes)
- Sync: `grotto-android/app/src/main/proto/grotto.proto` (exact same changes)

**Step 1: Add message types to enum**

In `grotto.proto`, after `MT_VOICE_SIGNAL = 40;`, add:

```protobuf
  MT_VOICE_ROOM_JOIN  = 41;  // User joining a voice room
  MT_VOICE_ROOM_LEAVE = 42;  // User leaving a voice room
  MT_VOICE_ROOM_STATE = 43;  // Full participant list (sent to joiner)
```

**Step 2: Add message definitions**

After the `VoiceSignal` message block (after line ~146), add:

```protobuf
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

**Step 3: Copy proto to client and android**

```bash
cp grotto-server/src/proto/grotto.proto grotto-client/src/proto/grotto.proto
cp grotto-server/src/proto/grotto.proto grotto-android/app/src/main/proto/grotto.proto
```

**Step 4: Rebuild proto**

```bash
cd grotto-server && cmake --build build
cd grotto-client && cmake --build build
```

**Step 5: Commit**

```bash
git add -A grotto-server/src/proto/ grotto-client/src/proto/ grotto-android/app/src/main/proto/
git commit -m "proto: add VoiceRoomJoin/Leave/State messages (MT 41-43)"
```

---

### Task 2: Server — VoiceRoomManager

**Files:**
- Create: `grotto-server/src/voice/voice_room_manager.hpp`
- Create: `grotto-server/src/voice/voice_room_manager.cpp`

**Step 1: Write the header**

```cpp
// grotto-server/src/voice/voice_room_manager.hpp
#pragma once

#include "grotto.pb.h"
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace grotto::net { class Session; }

namespace grotto::voice {

class VoiceRoomManager {
public:
    using SessionPtr = std::shared_ptr<net::Session>;
    using FindSessionFn = std::function<SessionPtr(const std::string&)>;

    explicit VoiceRoomManager(FindSessionFn find_session);

    // Returns error string on failure, empty on success.
    std::string join(const std::string& channel_id, const std::string& user_id);
    void leave(const std::string& channel_id, const std::string& user_id);
    void on_disconnect(const std::string& user_id);

    std::vector<std::string> participants(const std::string& channel_id) const;
    bool is_in_voice(const std::string& user_id) const;

    static constexpr size_t kMaxPerRoom = 8;

private:
    void send_to(const std::string& user_id, MessageType type,
                 const google::protobuf::Message& msg);
    void broadcast_to_room(const std::string& channel_id, MessageType type,
                           const google::protobuf::Message& msg,
                           const std::string& exclude = "");

    mutable std::mutex mu_;
    // channel_id -> set of user_ids
    std::unordered_map<std::string, std::set<std::string>> rooms_;
    // user_id -> channel_id (each user can be in at most one voice room)
    std::unordered_map<std::string, std::string> user_room_;

    FindSessionFn find_session_;
};

} // namespace grotto::voice
```

**Step 2: Write the implementation**

```cpp
// grotto-server/src/voice/voice_room_manager.cpp
#include "voice/voice_room_manager.hpp"
#include "net/session.hpp"
#include <spdlog/spdlog.h>

namespace grotto::voice {

VoiceRoomManager::VoiceRoomManager(FindSessionFn find_session)
    : find_session_(std::move(find_session)) {}

std::string VoiceRoomManager::join(const std::string& channel_id,
                                    const std::string& user_id) {
    std::lock_guard lk(mu_);

    // Already in a room? Auto-leave.
    if (auto it = user_room_.find(user_id); it != user_room_.end()) {
        // Unlock not needed — we hold the lock for the whole operation
        auto& old_room = rooms_[it->second];
        old_room.erase(user_id);

        VoiceRoomLeave leave_msg;
        leave_msg.set_channel_id(it->second);
        leave_msg.set_user_id(user_id);
        // Broadcast leave to old room (without lock — but we hold it, so we
        // call the internal helper which uses find_session_ directly)
        for (const auto& peer : old_room) {
            send_to(peer, MT_VOICE_ROOM_LEAVE, leave_msg);
        }

        if (old_room.empty()) rooms_.erase(it->second);
        user_room_.erase(it);
    }

    auto& room = rooms_[channel_id];

    if (room.size() >= kMaxPerRoom) {
        return "Voice room is full (max " + std::to_string(kMaxPerRoom) + ")";
    }

    // Notify existing participants that a new user joined
    VoiceRoomJoin join_msg;
    join_msg.set_channel_id(channel_id);
    join_msg.set_user_id(user_id);
    for (const auto& peer : room) {
        send_to(peer, MT_VOICE_ROOM_JOIN, join_msg);
    }

    // Add user to room
    room.insert(user_id);
    user_room_[user_id] = channel_id;

    // Send full participant list to the joiner
    VoiceRoomState state;
    state.set_channel_id(channel_id);
    for (const auto& p : room) {
        state.add_participants(p);
    }
    send_to(user_id, MT_VOICE_ROOM_STATE, state);

    spdlog::info("Voice: {} joined room {} ({} participants)",
                 user_id, channel_id, room.size());
    return {};
}

void VoiceRoomManager::leave(const std::string& channel_id,
                              const std::string& user_id) {
    std::lock_guard lk(mu_);

    auto room_it = rooms_.find(channel_id);
    if (room_it == rooms_.end()) return;

    auto& room = room_it->second;
    if (!room.erase(user_id)) return;

    user_room_.erase(user_id);

    VoiceRoomLeave leave_msg;
    leave_msg.set_channel_id(channel_id);
    leave_msg.set_user_id(user_id);
    for (const auto& peer : room) {
        send_to(peer, MT_VOICE_ROOM_LEAVE, leave_msg);
    }

    if (room.empty()) rooms_.erase(room_it);

    spdlog::info("Voice: {} left room {}", user_id, channel_id);
}

void VoiceRoomManager::on_disconnect(const std::string& user_id) {
    std::lock_guard lk(mu_);

    auto it = user_room_.find(user_id);
    if (it == user_room_.end()) return;

    std::string channel_id = it->second;
    auto room_it = rooms_.find(channel_id);
    if (room_it != rooms_.end()) {
        room_it->second.erase(user_id);

        VoiceRoomLeave leave_msg;
        leave_msg.set_channel_id(channel_id);
        leave_msg.set_user_id(user_id);
        for (const auto& peer : room_it->second) {
            send_to(peer, MT_VOICE_ROOM_LEAVE, leave_msg);
        }

        if (room_it->second.empty()) rooms_.erase(room_it);
    }

    user_room_.erase(it);
    spdlog::info("Voice: {} disconnected, removed from {}", user_id, channel_id);
}

std::vector<std::string> VoiceRoomManager::participants(
    const std::string& channel_id) const {
    std::lock_guard lk(mu_);
    auto it = rooms_.find(channel_id);
    if (it == rooms_.end()) return {};
    return {it->second.begin(), it->second.end()};
}

bool VoiceRoomManager::is_in_voice(const std::string& user_id) const {
    std::lock_guard lk(mu_);
    return user_room_.count(user_id) > 0;
}

void VoiceRoomManager::send_to(const std::string& user_id, MessageType type,
                                const google::protobuf::Message& msg) {
    auto session = find_session_(user_id);
    if (!session) return;
    Envelope env;
    env.set_type(type);
    msg.SerializeToString(env.mutable_payload());
    session->send(env);
}

void VoiceRoomManager::broadcast_to_room(const std::string& channel_id,
                                          MessageType type,
                                          const google::protobuf::Message& msg,
                                          const std::string& exclude) {
    auto it = rooms_.find(channel_id);
    if (it == rooms_.end()) return;
    for (const auto& user_id : it->second) {
        if (user_id != exclude) {
            send_to(user_id, type, msg);
        }
    }
}

} // namespace grotto::voice
```

**Step 3: Commit**

```bash
git add grotto-server/src/voice/
git commit -m "feat(server): add VoiceRoomManager for voice room membership"
```

---

### Task 3: Server — Wire VoiceRoomManager into Session + Listener

**Files:**
- Modify: `grotto-server/src/net/session.hpp:86` (add handler declarations)
- Modify: `grotto-server/src/net/session.hpp:150-183` (add voice_room_manager to ServerContext)
- Modify: `grotto-server/src/net/session.cpp:260` (add case branches in handle_envelope)
- Modify: `grotto-server/src/net/session.cpp` (add handler implementations)
- Modify: `grotto-server/src/net/listener.hpp` (add VoiceRoomManager member + accessor)
- Modify: `grotto-server/src/net/listener.cpp:215-240` (call on_disconnect in on_session_disconnected)

**Step 1: Add to ServerContext interface**

In `session.hpp`, add to the `ServerContext` class (after `command_handler()` line ~177):

```cpp
    virtual voice::VoiceRoomManager& voice_room_manager() = 0;
```

Add forward declaration at top of file (after line ~22):

```cpp
namespace grotto::voice { class VoiceRoomManager; }
```

**Step 2: Add handler declarations to Session**

In `session.hpp`, after `handle_voice_signal` (line ~86), add:

```cpp
    void handle_voice_room_join(const VoiceRoomJoin& join);
    void handle_voice_room_leave(const VoiceRoomLeave& leave);
```

**Step 3: Add case branches in Session::handle_envelope**

In `session.cpp`, after the `MT_VOICE_SIGNAL` case block (around line ~275), add:

```cpp
    case MT_VOICE_ROOM_JOIN:
        if (state_ == SessionState::Established) {
            VoiceRoomJoin join;
            if (!env.payload().empty() && join.ParseFromArray(
                    env.payload().data(), static_cast<int>(env.payload().size()))) {
                handle_voice_room_join(join);
            } else {
                send_error(4075, "Invalid VOICE_ROOM_JOIN");
            }
        }
        break;

    case MT_VOICE_ROOM_LEAVE:
        if (state_ == SessionState::Established) {
            VoiceRoomLeave leave;
            if (!env.payload().empty() && leave.ParseFromArray(
                    env.payload().data(), static_cast<int>(env.payload().size()))) {
                handle_voice_room_leave(leave);
            } else {
                send_error(4076, "Invalid VOICE_ROOM_LEAVE");
            }
        }
        break;
```

**Step 4: Add handler implementations**

In `session.cpp`, after `handle_voice_signal` (around line ~600), add:

```cpp
void Session::handle_voice_room_join(const VoiceRoomJoin& join) {
    std::string error = server_ctx_.voice_room_manager().join(
        join.channel_id(), user_id_);
    if (!error.empty()) {
        send_error(4077, error);
    }
}

void Session::handle_voice_room_leave(const VoiceRoomLeave& leave) {
    server_ctx_.voice_room_manager().leave(leave.channel_id(), user_id_);
}
```

**Step 5: Add VoiceRoomManager to Listener**

In `listener.hpp`, add include:

```cpp
#include "voice/voice_room_manager.hpp"
```

Add member (after `command_handler_` line ~94):

```cpp
    std::unique_ptr<voice::VoiceRoomManager> voice_room_mgr_;
```

Add accessor override (after `command_handler()` line ~53):

```cpp
    voice::VoiceRoomManager& voice_room_manager() override { return *voice_room_mgr_; }
```

**Step 6: Initialize VoiceRoomManager in Listener constructor**

In `listener.cpp`, in the constructor body, initialize:

```cpp
    voice_room_mgr_ = std::make_unique<voice::VoiceRoomManager>(
        [this](const std::string& uid) { return find_session(uid); });
```

**Step 7: Call on_disconnect in on_session_disconnected**

In `listener.cpp:on_session_disconnected`, after broadcasting OFFLINE presence (~line 238), add:

```cpp
        voice_room_mgr_->on_disconnect(disconnected_user_id);
```

**Step 8: Build and verify**

```bash
cd grotto-server && cmake --build build
```

**Step 9: Commit**

```bash
git add grotto-server/src/
git commit -m "feat(server): wire VoiceRoomManager into session dispatch + listener lifecycle"
```

---

### Task 4: Client — Handle Voice Room Messages in MessageHandler

**Files:**
- Modify: `grotto-client/src/net/message_handler.hpp:53` (add handler declarations)
- Modify: `grotto-client/src/net/message_handler.cpp` (add dispatch cases + handlers)

**Step 1: Add handler declarations**

In `message_handler.hpp`, after `handle_voice_signal` (line ~53), add:

```cpp
    void handle_voice_room_join(const Envelope& env);
    void handle_voice_room_leave(const Envelope& env);
    void handle_voice_room_state(const Envelope& env);
```

**Step 2: Add dispatch cases**

In `message_handler.cpp`, in the `dispatch()` switch, add cases for MT_VOICE_ROOM_JOIN, MT_VOICE_ROOM_LEAVE, MT_VOICE_ROOM_STATE:

```cpp
    case MT_VOICE_ROOM_JOIN:  handle_voice_room_join(env);  break;
    case MT_VOICE_ROOM_LEAVE: handle_voice_room_leave(env); break;
    case MT_VOICE_ROOM_STATE: handle_voice_room_state(env); break;
```

**Step 3: Implement handlers**

```cpp
void MessageHandler::handle_voice_room_state(const Envelope& env) {
    VoiceRoomState state;
    if (!state.ParseFromArray(env.payload().data(),
            static_cast<int>(env.payload().size()))) return;

    // Tell VoiceEngine to set up peer connections to all participants
    if (voice_engine_) {
        std::vector<std::string> peers;
        for (const auto& p : state.participants()) {
            if (p != cfg_.identity.user_id) {
                peers.push_back(p);
            }
        }
        voice_engine_->on_room_joined(state.channel_id(), peers);
    }

    state_.post_ui([this, state]() {
        VoiceState vs = state_.voice_snapshot();
        vs.in_voice = true;
        vs.active_channel = state.channel_id();
        vs.participants.clear();
        for (const auto& p : state.participants()) {
            vs.participants.push_back(p);
        }
        state_.set_voice_state(vs);
    });
}

void MessageHandler::handle_voice_room_join(const Envelope& env) {
    VoiceRoomJoin join;
    if (!join.ParseFromArray(env.payload().data(),
            static_cast<int>(env.payload().size()))) return;

    // Another user joined our room — create peer connection to them
    if (voice_engine_) {
        voice_engine_->on_peer_joined(join.user_id());
    }

    state_.post_ui([this, join]() {
        VoiceState vs = state_.voice_snapshot();
        vs.participants.push_back(join.user_id());
        state_.set_voice_state(vs);

        Message msg;
        msg.type = Message::Type::System;
        msg.sender_id = "system";
        msg.content = join.user_id() + " joined voice";
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_.push_message(join.channel_id(), std::move(msg));
    });
}

void MessageHandler::handle_voice_room_leave(const Envelope& env) {
    VoiceRoomLeave leave;
    if (!leave.ParseFromArray(env.payload().data(),
            static_cast<int>(env.payload().size()))) return;

    // Remove peer connection
    if (voice_engine_) {
        voice_engine_->on_peer_left(leave.user_id());
    }

    state_.post_ui([this, leave]() {
        VoiceState vs = state_.voice_snapshot();
        auto& p = vs.participants;
        p.erase(std::remove(p.begin(), p.end(), leave.user_id()), p.end());
        state_.set_voice_state(vs);

        Message msg;
        msg.type = Message::Type::System;
        msg.sender_id = "system";
        msg.content = leave.user_id() + " left voice";
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_.push_message(leave.channel_id(), std::move(msg));
    });
}
```

**Step 4: Build and verify**

```bash
cd grotto-client && cmake --build build
```

**Step 5: Commit**

```bash
git add grotto-client/src/net/
git commit -m "feat(client): handle VoiceRoomJoin/Leave/State in MessageHandler"
```

---

### Task 5: Client — VoiceEngine Room Coordination Methods

**Files:**
- Modify: `grotto-client/src/voice/voice_engine.hpp:41-43` (add new methods)
- Modify: `grotto-client/src/voice/voice_engine.cpp` (implement new methods)

**Step 1: Add method declarations**

In `voice_engine.hpp`, after `leave_room()` (line ~43), add:

```cpp
    // Called by MessageHandler when server sends VoiceRoomState (we joined)
    void on_room_joined(const std::string& channel_id,
                        const std::vector<std::string>& peers);

    // Called by MessageHandler when another user joins our room
    void on_peer_joined(const std::string& peer_id);

    // Called by MessageHandler when another user leaves our room
    void on_peer_left(const std::string& peer_id);
```

**Step 2: Implement**

In `voice_engine.cpp`:

```cpp
void VoiceEngine::on_room_joined(const std::string& channel_id,
                                  const std::vector<std::string>& peers) {
    if (in_voice_) leave_room();

    active_channel_ = channel_id;
    in_voice_       = true;
    muted_          = false;
    deafened_       = false;

    audio_.open(cfg_.voice.input_device, cfg_.voice.output_device,
        [this](const float* pcm, uint32_t frames) { on_capture(pcm, frames); },
        [this](float* out, uint32_t frames)        { mix_output(out, frames); });
    audio_.start();

    // We are the joiner — create PeerConnections to all existing participants as offerer
    for (const auto& peer_id : peers) {
        get_or_create_peer(peer_id, /*is_offerer=*/true);
    }

    spdlog::info("Voice room joined: {} with {} peers", channel_id, peers.size());
}

void VoiceEngine::on_peer_joined(const std::string& peer_id) {
    if (!in_voice_) return;
    // New peer joined — they will offer to us, we wait for their OFFER.
    // No action needed here — the peer will create PeerConnection as offerer
    // and we'll handle it in on_voice_signal(OFFER).
    spdlog::info("Peer {} joined voice room", peer_id);
}

void VoiceEngine::on_peer_left(const std::string& peer_id) {
    if (!in_voice_) return;
    std::lock_guard lk(mu_);
    peers_.erase(peer_id);
    spdlog::info("Peer {} left voice room", peer_id);
}
```

**Step 3: Update `join_room` to send VoiceRoomJoin to server**

Replace the existing `join_room` method body to send the room join message to the server instead of directly opening audio (audio is now opened in `on_room_joined`):

```cpp
void VoiceEngine::join_room(const std::string& channel_id) {
    // Send VoiceRoomJoin to server — audio setup happens in on_room_joined()
    // when we receive VoiceRoomState back
    VoiceRoomJoin join;
    join.set_channel_id(channel_id);
    // send_signal_ is reused — we need a new send function for room messages.
    // Use a dedicated room_send_fn_ or piggyback on send_signal_.
    // For simplicity, add a new send function.
    if (send_room_msg_) send_room_msg_(MT_VOICE_ROOM_JOIN, join);
}
```

**Step 4: Update `leave_room` to send VoiceRoomLeave**

Update `leave_room`:

```cpp
void VoiceEngine::leave_room() {
    if (!in_voice_) return;

    std::string channel = active_channel_;

    audio_.stop();

    {
        std::lock_guard lk(mu_);
        peers_.clear();
    }

    in_voice_       = false;
    active_channel_.clear();

    VoiceState vs = state_.voice_snapshot();
    vs.in_voice        = false;
    vs.active_channel  = {};
    vs.participants    = {};
    state_.set_voice_state(vs);

    // Notify server
    VoiceRoomLeave leave;
    leave.set_channel_id(channel);
    if (send_room_msg_) send_room_msg_(MT_VOICE_ROOM_LEAVE, leave);
}
```

**Step 5: Add send_room_msg_ function**

In `voice_engine.hpp`, add:

```cpp
    using SendRoomMsgFn = std::function<void(MessageType, const google::protobuf::Message&)>;
    void set_send_room_msg(SendRoomMsgFn fn) { send_room_msg_ = std::move(fn); }
```

In the private section, add:

```cpp
    SendRoomMsgFn send_room_msg_;
```

**Step 6: Wire in App**

In `app.cpp`, after `voice_->set_send_signal(...)`, add:

```cpp
    voice_->set_send_room_msg([this](MessageType type, const google::protobuf::Message& msg) {
        Envelope env;
        env.set_type(type);
        msg.SerializeToString(env.mutable_payload());
        net_client_->send(env);
    });
```

**Step 7: Update /voice command handler in app.cpp**

Replace the `/voice` handler (lines ~218-226) with:

```cpp
    } else if (cmd.name == "/voice") {
        if (!cmd.args.empty() && cmd.args[0] == "leave") {
            voice_->leave_room();
            ui_->push_system_msg("Left voice room.");
        } else if (voice_->in_voice()) {
            voice_->leave_room();
            ui_->push_system_msg("Left voice room.");
        } else {
            std::string ch = cmd.args.empty()
                ? state_.active_channel().value_or("#general")
                : cmd.args[0];
            voice_->join_room(ch);
            ui_->push_system_msg("Joining voice room: " + ch + "...");
        }
```

**Step 8: Build and verify**

```bash
cd grotto-client && cmake --build build
```

**Step 9: Commit**

```bash
git add grotto-client/src/
git commit -m "feat(client): voice room coordination — join/leave/peer lifecycle"
```

---

### Task 6: Client — PTT and VOX Input Modes

**Files:**
- Modify: `grotto-client/src/config.hpp:25-30` (add PTT/VOX config fields)
- Modify: `grotto-client/src/config.cpp` (parse new fields)
- Modify: `grotto-client/src/voice/voice_engine.hpp` (add PTT/VOX state)
- Modify: `grotto-client/src/voice/voice_engine.cpp:302-325` (gate on_capture with PTT/VOX)
- Modify: `grotto-client/src/ui/ui_manager.cpp` (capture F1 key for PTT)
- Modify: `grotto-client/src/app.cpp` (add /ptt command)

**Step 1: Add config fields**

In `config.hpp`, update `VoiceConfig`:

```cpp
struct VoiceConfig {
    std::string input_device;
    std::string output_device;
    int         opus_bitrate   = 64000;
    int         frame_ms       = 20;
    std::string mode           = "ptt";     // "ptt" or "vox"
    std::string ptt_key        = "F1";
    float       vad_threshold  = 0.02f;
};
```

**Step 2: Parse in config.cpp**

Add parsing for the new fields (alongside existing voice config parsing):

```cpp
    cfg.voice.mode          = voice_tbl.at_or("mode", "ptt");
    cfg.voice.ptt_key       = voice_tbl.at_or("ptt_key", "F1");
    cfg.voice.vad_threshold = voice_tbl.at_or("vad_threshold", 0.02);
```

**Step 3: Add PTT/VOX state to VoiceEngine**

In `voice_engine.hpp`, add to private members:

```cpp
    bool ptt_active_ = false;  // true while PTT key is held
    std::string voice_mode_ = "ptt"; // "ptt" or "vox"
```

Add public methods:

```cpp
    void set_ptt_active(bool active) { ptt_active_ = active; }
    void toggle_voice_mode();
    const std::string& voice_mode() const { return voice_mode_; }
```

**Step 4: Update on_capture with PTT/VOX gating**

In `voice_engine.cpp`, modify `on_capture`:

```cpp
void VoiceEngine::on_capture(const float* pcm, uint32_t frames) {
    if (muted_ || !in_voice_) return;

    // PTT/VOX gate
    if (voice_mode_ == "ptt" && !ptt_active_) return;

    if (voice_mode_ == "vox") {
        // Simple energy-based VAD
        float energy = 0.0f;
        uint32_t n = std::min(frames, static_cast<uint32_t>(OpusCodec::kFrameSamples));
        for (uint32_t i = 0; i < n; ++i) {
            energy += pcm[i] * pcm[i];
        }
        energy = std::sqrt(energy / n);
        if (energy < cfg_.voice.vad_threshold) return;
    }

    // ... rest of existing encode+send logic unchanged ...
```

**Step 5: Implement toggle_voice_mode**

```cpp
void VoiceEngine::toggle_voice_mode() {
    voice_mode_ = (voice_mode_ == "ptt") ? "vox" : "ptt";
    VoiceState vs = state_.voice_snapshot();
    state_.set_voice_state(vs); // trigger UI refresh
}
```

**Step 6: Initialize voice_mode_ from config**

In the VoiceEngine constructor:

```cpp
VoiceEngine::VoiceEngine(AppState& state, const ClientConfig& cfg)
    : state_(state), cfg_(cfg), voice_mode_(cfg.voice.mode) {}
```

**Step 7: Capture PTT key in FTXUI event loop**

In `ui_manager.cpp`, in the key event handler, add F1 key handling. F1 in FTXUI is `ftxui::Event::F1`. When pressed, call `voice_engine->set_ptt_active(true)`. On release... FTXUI doesn't have key-up events natively. Alternative: use a keyboard hook or toggle approach.

**Pragmatic approach**: Since FTXUI doesn't support key-release events, use a **toggle** for PTT instead of hold-to-talk:
- Press F1 once to start transmitting
- Press F1 again to stop

In the event handler:

```cpp
if (event == ftxui::Event::F1) {
    if (voice_engine_) {
        ptt_toggled_ = !ptt_toggled_;
        voice_engine_->set_ptt_active(ptt_toggled_);
    }
    return true;
}
```

**Step 8: Add /ptt command in app.cpp**

```cpp
    } else if (cmd.name == "/ptt") {
        voice_->toggle_voice_mode();
        ui_->push_system_msg("Voice mode: " + voice_->voice_mode());
```

**Step 9: Build and verify**

```bash
cd grotto-client && cmake --build build
```

**Step 10: Commit**

```bash
git add grotto-client/src/
git commit -m "feat(client): PTT toggle + VOX energy-based VAD input modes"
```

---

### Task 7: Client — Status Bar Voice Indicator

**Files:**
- Modify: `grotto-client/src/ui/status_bar.hpp:8-16` (add voice_mode field)
- Modify: `grotto-client/src/ui/status_bar.cpp` (render voice indicator)
- Modify: `grotto-client/src/ui/ui_manager.cpp` (populate StatusInfo with voice data)

**Step 1: Add voice_mode to StatusInfo**

In `status_bar.hpp`:

```cpp
struct StatusInfo {
    bool        connected      = false;
    std::string local_user_id;
    std::string active_channel;
    bool        in_voice       = false;
    bool        muted          = false;
    bool        deafened       = false;
    std::string voice_channel;
    std::string voice_mode     = "ptt";  // "ptt" or "vox"
    std::vector<std::string> voice_participants;
    std::vector<std::string> online_users;
};
```

**Step 2: Update render_status_bar**

In `status_bar.cpp`, add a voice section to the status bar rendering:

```cpp
if (info.in_voice) {
    std::string voice_text = "Voice: " + info.voice_channel +
        " " + std::to_string(info.voice_participants.size()) + " users";
    if (info.muted) voice_text += " [MUTED]";
    if (info.deafened) voice_text += " [DEAF]";
    voice_text += " [" + (info.voice_mode == "ptt" ? "PTT: F1" : "VOX") + "]";

    elements.push_back(separator());
    elements.push_back(text(voice_text) | color(Color::Green));
}
```

**Step 3: Populate StatusInfo in ui_manager.cpp**

When building `StatusInfo`, pull from `VoiceState`:

```cpp
auto vs = state_.voice_snapshot();
status.in_voice          = vs.in_voice;
status.muted             = vs.muted;
status.deafened          = vs.deafened;
status.voice_channel     = vs.active_channel;
status.voice_participants = vs.participants;
// voice_mode from voice engine (need accessor)
```

**Step 4: Build and verify**

```bash
cd grotto-client && cmake --build build
```

**Step 5: Commit**

```bash
git add grotto-client/src/ui/
git commit -m "feat(client): voice indicator in status bar with PTT/VOX mode display"
```

---

### Task 8: Update TODO and Documentation

**Files:**
- Modify: `TODO.md` (mark voice rooms as done)
- Modify: `docs/plans/2026-03-11-voice-rooms-design.md` (mark as implemented)

**Step 1: Update TODO.md**

Mark voice room items as complete:

```markdown
- [x] **Voice rooms**
  - [x] Join/leave voice channels
  - [x] Push-to-talk or voice activation
  - [x] Participant list with speaking indicators
  - [x] Mute/deafen controls
```

**Step 2: Commit**

```bash
git add TODO.md docs/
git commit -m "docs: mark voice rooms as implemented, update plan"
```
