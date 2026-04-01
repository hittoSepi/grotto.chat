#pragma once

#include "voice/opus_codec.hpp"
#include "voice/jitter_buffer.hpp"
#include "voice/audio_device.hpp"
#include "voice/pcm_sample_fifo.hpp"
#include "state/app_state.hpp"
#include "config.hpp"
#include "grotto.pb.h"

#include <rtc/rtc.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace grotto::voice {

// Callback to send a VoiceSignal envelope through the network layer
using SendSignalFn = std::function<void(const VoiceSignal&)>;
using SendRoomMsgFn = std::function<void(MessageType, const google::protobuf::Message&)>;

// Per-peer connection state
struct PeerConn {
    std::shared_ptr<rtc::PeerConnection>  pc;
    std::shared_ptr<rtc::Track>           send_track;
    std::shared_ptr<rtc::Track>           recv_track;
    JitterBuffer                          jitter_buf;
    PcmSampleFifo                         playout_fifo;
    OpusCodec                             codec;
    std::string                           peer_id;
    bool                                  connected = false;
    bool                                  room_offer_local = false;
    bool                                  recv_track_seen = false;
    bool                                  no_media_warning_logged = false;
    uint64_t                              tx_packets = 0;
    uint64_t                              rx_packets = 0;
    uint64_t                              decoded_frames = 0;
    float                                 last_energy = 0.0f;  // for speaking indicator
    std::chrono::steady_clock::time_point last_packet_time;
    std::chrono::steady_clock::time_point connected_since;
};

enum class VoiceSessionKind {
    None,
    Room,
    Direct,
};

class VoiceEngine {
public:
    VoiceEngine(AppState& state, const ClientConfig& cfg);
    ~VoiceEngine();

    void set_send_signal(SendSignalFn fn) { send_signal_ = std::move(fn); }
    void set_send_room_msg(SendRoomMsgFn fn) { send_room_msg_ = std::move(fn); }

    // ── Room (multi-party) ────────────────────────────────────────────────
    void join_room(const std::string& channel_id);
    void leave_room();

    // Called by MessageHandler when server sends VoiceRoomState (we joined)
    void on_room_joined(const std::string& channel_id,
                        const std::vector<std::string>& peers);

    // Called by MessageHandler when another user joins our room
    void on_peer_joined(const std::string& peer_id);

    // Called by MessageHandler when another user leaves our room
    void on_peer_left(const std::string& peer_id);

    // ── 1:1 call ──────────────────────────────────────────────────────────
    void call(const std::string& peer_id);
    void accept_call(const std::string& caller_id);
    void hangup();

    // ── Controls ──────────────────────────────────────────────────────────
    void set_muted(bool muted);
    void set_deafened(bool deafened);
    void set_ptt_active(bool active) { ptt_active_.store(active, std::memory_order_relaxed); }
    void toggle_voice_mode();
    const std::string& voice_mode() const { return voice_mode_; }

    // ── Signaling (called from MessageHandler) ───────────────────────────
    void on_voice_signal(const VoiceSignal& vs);

    // ── Audio callbacks (called from AudioDevice thread) ─────────────────
    void on_capture(const float* pcm, uint32_t frames);
    void mix_output(float* out, uint32_t frames);

    bool in_voice() const { return in_voice_.load(std::memory_order_relaxed); }

    // Get list of peer IDs that are currently speaking (energy above threshold)
    std::vector<std::string> get_speaking_peers() const;

    // Update VoiceState with current speaking peers (call from UI thread periodically)
    void refresh_speaking_state();

private:
    std::shared_ptr<PeerConn> get_or_create_peer(const std::string& peer_id,
                                                   bool is_offerer);
    void ensure_send_track(const std::shared_ptr<PeerConn>& peer);
    void setup_peer_callbacks(std::shared_ptr<PeerConn> peer);
    rtc::Configuration make_rtc_config();
    bool open_audio_or_report(const std::string& failure_context);
    void set_voice_state_for_session(const std::string& active_channel,
                                     const std::vector<std::string>& participants);
    void reset_voice_state();
    void clear_participant_voice_statuses(const std::vector<std::string>& participants);
    void push_voice_event_to_channel(const std::string& channel_id, const std::string& text);
    void end_current_session(bool notify_remote_hangup, const std::string& remote_peer = {});

    AppState&          state_;
    const ClientConfig& cfg_;
    SendSignalFn       send_signal_;
    SendRoomMsgFn      send_room_msg_;

    mutable std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<PeerConn>> peers_;

    AudioDevice        audio_;
    std::atomic_bool   in_voice_{false};
    std::atomic_bool   muted_{false};
    std::atomic_bool   deafened_{false};
    std::atomic_bool   ptt_active_{false};
    VoiceSessionKind   session_kind_ = VoiceSessionKind::None;
    std::string        active_channel_;
    std::string        voice_mode_;
    PcmSampleFifo      capture_fifo_;
    std::atomic_bool   logged_first_capture_chunk_{false};

    uint16_t           rtp_seq_ = 0;
};

} // namespace grotto::voice
