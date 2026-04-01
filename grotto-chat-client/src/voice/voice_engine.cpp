#include "voice/voice_engine.hpp"
#include "voice/voice_peer_role.hpp"
#include "i18n/strings.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <variant>

namespace grotto::voice {

namespace {

const char* rtc_state_name(rtc::PeerConnection::State state) {
    switch (state) {
    case rtc::PeerConnection::State::New: return "new";
    case rtc::PeerConnection::State::Connecting: return "connecting";
    case rtc::PeerConnection::State::Connected: return "connected";
    case rtc::PeerConnection::State::Disconnected: return "disconnected";
    case rtc::PeerConnection::State::Failed: return "failed";
    case rtc::PeerConnection::State::Closed: return "closed";
    default: return "unknown";
    }
}

const char* ice_config_source_name(bool from_server,
                                   bool has_runtime_ice,
                                   bool has_local_ice) {
    if (from_server && has_runtime_ice) {
        return "server";
    }
    if (has_local_ice) {
        return "config";
    }
    return "builtin-stun";
}

} // namespace

VoiceEngine::VoiceEngine(AppState& state, const ClientConfig& cfg)
    : state_(state), cfg_(cfg), voice_mode_(cfg.voice.mode) {}

VoiceEngine::~VoiceEngine() {
    hangup();
    audio_.close();
}

rtc::Configuration VoiceEngine::make_rtc_config() {
    rtc::Configuration config;
    auto runtime_ice = state_.runtime_voice_ice_config();
    const bool has_runtime_ice = !runtime_ice.ice_servers.empty();
    const bool has_local_ice = !cfg_.voice.ice_servers.empty();
    const auto& ice_servers = has_runtime_ice
        ? runtime_ice.ice_servers
        : cfg_.voice.ice_servers;
    const auto& turn_username = !runtime_ice.turn_username.empty()
        ? runtime_ice.turn_username
        : cfg_.voice.turn_username;
    const auto& turn_password = !runtime_ice.turn_password.empty()
        ? runtime_ice.turn_password
        : cfg_.voice.turn_password;

    spdlog::info("Building RTC config from {} (ice_servers={}, turn_user={})",
                 ice_config_source_name(runtime_ice.from_server, has_runtime_ice, has_local_ice),
                 ice_servers.size(),
                 turn_username.empty() ? "<none>" : turn_username);

    if (ice_servers.empty()) {
        // Default STUN servers
        config.iceServers.push_back({"stun:stun.l.google.com:19302"});
        config.iceServers.push_back({"stun:stun1.l.google.com:19302"});
    } else {
        for (const auto& server : ice_servers) {
            rtc::IceServer ice(server);
            if (!turn_username.empty()) {
                ice.username = turn_username;
                ice.password = turn_password;
            }
            config.iceServers.push_back(std::move(ice));
        }
    }
    return config;
}

bool VoiceEngine::open_audio_or_report(const std::string& failure_context) {
    if (audio_.open(cfg_.voice.input_device, cfg_.voice.output_device,
            [this](const float* pcm, uint32_t frames) { on_capture(pcm, frames); },
            [this](float* out, uint32_t frames) { mix_output(out, frames); })) {
        audio_.start();
        return true;
    }

    spdlog::error("Failed to open audio device for {}", failure_context);
    state_.post_ui([this]() {
        Message msg;
        msg.type = Message::Type::System;
        msg.sender_id = "voice";
        msg.content = i18n::tr(i18n::I18nKey::FAILED_OPEN_AUDIO_DEVICE);
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto ch = state_.active_channel().value_or("server");
        state_.push_message(ch, std::move(msg));
    });
    return false;
}

void VoiceEngine::clear_participant_voice_statuses(const std::vector<std::string>& participants) {
    for (const auto& participant : participants) {
        state_.set_user_voice_status(participant, ChannelUserInfo::VoiceStatus::Off);
    }
}

void VoiceEngine::set_voice_state_for_session(const std::string& active_channel,
                                              const std::vector<std::string>& participants) {
    if (session_kind_ == VoiceSessionKind::Direct && !active_channel.empty()) {
        state_.ensure_channel(active_channel);
        state_.set_active_channel(active_channel);
        ChannelUserInfo peer_info;
        peer_info.user_id = active_channel;
        peer_info.presence = state_.presence(active_channel);
        state_.add_channel_user(active_channel, peer_info);
    }

    VoiceState vs = state_.voice_snapshot();
    clear_participant_voice_statuses(vs.participants);
    vs.in_voice = true;
    vs.muted = muted_.load(std::memory_order_relaxed);
    vs.deafened = deafened_.load(std::memory_order_relaxed);
    vs.active_channel = active_channel;
    vs.participants = participants;
    vs.speaking_peers.clear();
    vs.voice_mode = voice_mode_;
    state_.set_voice_state(std::move(vs));

    for (const auto& participant : participants) {
        state_.set_user_voice_status(participant, ChannelUserInfo::VoiceStatus::Off);
    }
}

void VoiceEngine::reset_voice_state() {
    VoiceState vs = state_.voice_snapshot();
    clear_participant_voice_statuses(vs.participants);
    vs.in_voice = false;
    vs.muted = false;
    vs.deafened = false;
    vs.active_channel.clear();
    vs.participants.clear();
    vs.speaking_peers.clear();
    vs.voice_mode = voice_mode_;
    state_.set_voice_state(std::move(vs));
}

void VoiceEngine::push_voice_event_to_channel(const std::string& channel_id, const std::string& text) {
    state_.post_ui([this, channel_id, text]() {
        state_.ensure_channel(channel_id);
        Message msg;
        msg.type = Message::Type::VoiceEvent;
        msg.sender_id = "voice";
        msg.content = text;
        msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_.push_message(channel_id, std::move(msg));
    });
}

void VoiceEngine::end_current_session(bool notify_remote_hangup, const std::string& remote_peer) {
    if (!in_voice_.load(std::memory_order_relaxed)) {
        return;
    }

    const auto previous_state = state_.voice_snapshot();
    const auto previous_kind = session_kind_;
    const std::string session_target = active_channel_;

    audio_.stop();
    capture_fifo_.clear();
    logged_first_capture_chunk_.store(false, std::memory_order_relaxed);

    {
        std::lock_guard lk(mu_);
        peers_.clear();
    }

    in_voice_.store(false, std::memory_order_relaxed);
    muted_.store(false, std::memory_order_relaxed);
    deafened_.store(false, std::memory_order_relaxed);
    ptt_active_.store(false, std::memory_order_relaxed);
    session_kind_ = VoiceSessionKind::None;
    active_channel_.clear();

    reset_voice_state();

    if (previous_kind == VoiceSessionKind::Room && !session_target.empty()) {
        VoiceRoomLeave leave;
        leave.set_channel_id(session_target);
        if (send_room_msg_) {
            send_room_msg_(MT_VOICE_ROOM_LEAVE, leave);
        }
    }

    if (notify_remote_hangup && previous_kind == VoiceSessionKind::Direct && !remote_peer.empty()) {
        VoiceSignal hup;
        hup.set_from_user(state_.local_user_id());
        hup.set_to_user(remote_peer);
        hup.set_signal_type(VoiceSignal::CALL_HANGUP);
        if (send_signal_) {
            send_signal_(hup);
        }
    }

    clear_participant_voice_statuses(previous_state.participants);
}

// ── Room ──────────────────────────────────────────────────────────────────────

void VoiceEngine::join_room(const std::string& channel_id) {
    // Send VoiceRoomJoin to server — audio setup happens in on_room_joined()
    // when we receive VoiceRoomState back
    VoiceRoomJoin join;
    join.set_channel_id(channel_id);
    if (send_room_msg_) send_room_msg_(MT_VOICE_ROOM_JOIN, join);
}

void VoiceEngine::on_room_joined(const std::string& channel_id,
                                  const std::vector<std::string>& peers) {
    if (in_voice_.load(std::memory_order_relaxed)) {
        end_current_session(true);
    }

    active_channel_ = channel_id;
    in_voice_.store(true, std::memory_order_relaxed);
    muted_.store(false, std::memory_order_relaxed);
    deafened_.store(false, std::memory_order_relaxed);
    ptt_active_.store(false, std::memory_order_relaxed);
    session_kind_   = VoiceSessionKind::Room;

    if (!open_audio_or_report("voice room")) {
        in_voice_.store(false, std::memory_order_relaxed);
        session_kind_ = VoiceSessionKind::None;
        active_channel_.clear();
        return;
    }

    capture_fifo_.clear();
    logged_first_capture_chunk_.store(false, std::memory_order_relaxed);

    for (const auto& peer_id : peers) {
        get_or_create_peer(peer_id, should_offer_to_peer(state_.local_user_id(), peer_id));
    }

    set_voice_state_for_session(channel_id, peers);

    spdlog::info("Voice room joined: {} with {} peers", channel_id, peers.size());
}

void VoiceEngine::on_peer_joined(const std::string& peer_id) {
    if (!in_voice_.load(std::memory_order_relaxed)) return;
    get_or_create_peer(peer_id, should_offer_to_peer(state_.local_user_id(), peer_id));
    spdlog::info("Peer {} joined voice room", peer_id);
}

void VoiceEngine::on_peer_left(const std::string& peer_id) {
    if (!in_voice_.load(std::memory_order_relaxed)) return;
    {
        std::lock_guard lk(mu_);
        peers_.erase(peer_id);
    }
    auto vs = state_.voice_snapshot();
    auto& participants = vs.participants;
    participants.erase(std::remove(participants.begin(), participants.end(), peer_id), participants.end());
    vs.speaking_peers.erase(
        std::remove(vs.speaking_peers.begin(), vs.speaking_peers.end(), peer_id),
        vs.speaking_peers.end());
    state_.set_voice_state(std::move(vs));
    state_.set_user_voice_status(peer_id, ChannelUserInfo::VoiceStatus::Off);
    spdlog::info("Peer {} left voice room", peer_id);
}

void VoiceEngine::leave_room() {
    if (!in_voice_.load(std::memory_order_relaxed)) {
        return;
    }
    if (session_kind_ == VoiceSessionKind::Direct) {
        end_current_session(true, active_channel_);
        return;
    }
    end_current_session(false);
}

// ── 1:1 call ──────────────────────────────────────────────────────────────────

void VoiceEngine::call(const std::string& peer_id) {
    if (in_voice_.load(std::memory_order_relaxed)) {
        end_current_session(true, active_channel_);
    }

    in_voice_.store(true, std::memory_order_relaxed);
    active_channel_ = peer_id;
    muted_.store(false, std::memory_order_relaxed);
    deafened_.store(false, std::memory_order_relaxed);
    ptt_active_.store(false, std::memory_order_relaxed);
    session_kind_   = VoiceSessionKind::Direct;

    if (!open_audio_or_report("call")) {
        in_voice_.store(false, std::memory_order_relaxed);
        session_kind_ = VoiceSessionKind::None;
        active_channel_.clear();
        return;
    }
    set_voice_state_for_session(peer_id, {peer_id});

    // Send CALL_INVITE
    VoiceSignal invite;
    invite.set_from_user(state_.local_user_id());
    invite.set_to_user(peer_id);
    invite.set_signal_type(VoiceSignal::CALL_INVITE);
    if (send_signal_) send_signal_(invite);

    spdlog::info("Calling {}", peer_id);
}

void VoiceEngine::accept_call(const std::string& caller_id) {
    if (in_voice_.load(std::memory_order_relaxed) &&
        !(session_kind_ == VoiceSessionKind::Direct && active_channel_ == caller_id)) {
        end_current_session(true, active_channel_);
    }

    in_voice_.store(true, std::memory_order_relaxed);
    active_channel_ = caller_id;
    muted_.store(false, std::memory_order_relaxed);
    deafened_.store(false, std::memory_order_relaxed);
    ptt_active_.store(false, std::memory_order_relaxed);
    session_kind_   = VoiceSessionKind::Direct;

    if (!open_audio_or_report("accepted call")) {
        in_voice_.store(false, std::memory_order_relaxed);
        session_kind_ = VoiceSessionKind::None;
        active_channel_.clear();
        return;
    }
    set_voice_state_for_session(caller_id, {caller_id});

    // Create PeerConnection as answerer
    get_or_create_peer(caller_id, /*is_offerer=*/false);

    VoiceSignal accept;
    accept.set_from_user(state_.local_user_id());
    accept.set_to_user(caller_id);
    accept.set_signal_type(VoiceSignal::CALL_ACCEPT);
    if (send_signal_) send_signal_(accept);
}

void VoiceEngine::hangup() {
    if (!in_voice_.load(std::memory_order_relaxed)) {
        return;
    }
    end_current_session(true, active_channel_);
}

// ── Controls ─────────────────────────────────────────────────────────────────

void VoiceEngine::set_muted(bool muted) {
    muted_.store(muted, std::memory_order_relaxed);
    VoiceState vs = state_.voice_snapshot();
    vs.muted = muted;
    state_.set_voice_state(vs);
}

void VoiceEngine::set_deafened(bool deafened) {
    deafened_.store(deafened, std::memory_order_relaxed);
    VoiceState vs = state_.voice_snapshot();
    vs.deafened = deafened;
    state_.set_voice_state(vs);
}

// ── Signaling ─────────────────────────────────────────────────────────────────

void VoiceEngine::on_voice_signal(const VoiceSignal& vs) {
    const std::string& from = vs.from_user();
    const std::string& sdp  = vs.sdp_or_ice();

    switch (vs.signal_type()) {
    case VoiceSignal::CALL_INVITE:
        spdlog::info("Incoming call from {}", from);
        push_voice_event_to_channel(from, i18n::tr(i18n::I18nKey::INCOMING_CALL, from));
        break;

    case VoiceSignal::CALL_ACCEPT: {
        get_or_create_peer(from, true);
        set_voice_state_for_session(from, {from});
        break;
    }

    case VoiceSignal::CALL_HANGUP:
        spdlog::info("{} hung up", from);
        if (session_kind_ == VoiceSessionKind::Direct && active_channel_ == from) {
            push_voice_event_to_channel(from, i18n::tr(i18n::I18nKey::CALL_ENDED));
            end_current_session(false);
        } else {
            std::lock_guard lk(mu_);
            peers_.erase(from);
            state_.set_user_voice_status(from, ChannelUserInfo::VoiceStatus::Off);
        }
        break;

    case VoiceSignal::OFFER: {
        spdlog::debug("Received OFFER from {}", from);
        auto peer = get_or_create_peer(from, false);
        if (peer && peer->pc) {
            try {
                ensure_send_track(peer);
                peer->pc->setRemoteDescription(
                    rtc::Description(sdp, rtc::Description::Type::Offer));
                peer->pc->setLocalDescription(rtc::Description::Type::Answer);
            } catch (const std::exception& e) {
                spdlog::debug("Ignoring OFFER from {} in current signaling state: {}",
                              from, e.what());
            }
        }
        break;
    }

    case VoiceSignal::ANSWER: {
        spdlog::debug("Received ANSWER from {}", from);
        std::shared_ptr<PeerConn> peer;
        {
            std::lock_guard lk(mu_);
            auto it = peers_.find(from);
            if (it != peers_.end()) peer = it->second;
        }
        if (peer && peer->pc) {
            try {
                peer->pc->setRemoteDescription(
                    rtc::Description(sdp, rtc::Description::Type::Answer));
            } catch (const std::exception& e) {
                spdlog::debug("Ignoring ANSWER from {} in current signaling state: {}",
                              from, e.what());
            }
        }
        break;
    }

    case VoiceSignal::ICE_CANDIDATE: {
        spdlog::debug("Received ICE candidate from {}", from);
        std::shared_ptr<PeerConn> peer;
        {
            std::lock_guard lk(mu_);
            auto it = peers_.find(from);
            if (it != peers_.end()) peer = it->second;
        }
        if (peer && peer->pc) {
            try {
                peer->pc->addRemoteCandidate(rtc::Candidate(sdp));
            } catch (const std::exception& e) {
                spdlog::debug("Ignoring ICE candidate from {} in current signaling state: {}",
                              from, e.what());
            }
        }
        break;
    }

    default: break;
    }
}

// ── Peer setup ────────────────────────────────────────────────────────────────

std::shared_ptr<PeerConn> VoiceEngine::get_or_create_peer(
    const std::string& peer_id, bool is_offerer)
{
    std::lock_guard lk(mu_);
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) return it->second;

    auto peer     = std::make_shared<PeerConn>();
    peer->peer_id = peer_id;
    peer->pc      = std::make_shared<rtc::PeerConnection>(make_rtc_config());
    peer->room_offer_local = (session_kind_ == VoiceSessionKind::Room) &&
        should_offer_to_peer(state_.local_user_id(), peer_id);

    peers_[peer_id] = peer;
    setup_peer_callbacks(peer);

    if (is_offerer) {
        ensure_send_track(peer);
        peer->pc->setLocalDescription();
    }

    return peer;
}

void VoiceEngine::ensure_send_track(const std::shared_ptr<PeerConn>& peer) {
    if (!peer || !peer->pc || peer->send_track) {
        return;
    }

    auto desc = rtc::Description::Audio("audio", rtc::Description::Direction::SendRecv);
    desc.addOpusCodec(111);
    peer->send_track = peer->pc->addTrack(std::move(desc));
}

void VoiceEngine::setup_peer_callbacks(std::shared_ptr<PeerConn> peer) {
    auto peer_id = peer->peer_id;

    peer->pc->onLocalDescription([this, peer_id](rtc::Description desc) {
        VoiceSignal sig;
        sig.set_from_user(state_.local_user_id());
        sig.set_to_user(peer_id);
        sig.set_sdp_or_ice(std::string(desc));
        sig.set_signal_type(desc.type() == rtc::Description::Type::Offer ?
                     VoiceSignal::OFFER : VoiceSignal::ANSWER);
        spdlog::debug("Sending {} to {}",
                      desc.type() == rtc::Description::Type::Offer ? "OFFER" : "ANSWER",
                      peer_id);
        if (send_signal_) send_signal_(sig);
    });

    peer->pc->onLocalCandidate([this, peer_id](rtc::Candidate cand) {
        VoiceSignal sig;
        sig.set_from_user(state_.local_user_id());
        sig.set_to_user(peer_id);
        sig.set_sdp_or_ice(std::string(cand));
        sig.set_signal_type(VoiceSignal::ICE_CANDIDATE);
        spdlog::debug("Sending ICE candidate to {}", peer_id);
        if (send_signal_) send_signal_(sig);
    });

    peer->pc->onStateChange([this, peer_id, peer](rtc::PeerConnection::State state) {
        spdlog::info("WebRTC state for {} -> {}", peer_id, rtc_state_name(state));
        if (state == rtc::PeerConnection::State::Connected) {
            spdlog::info("WebRTC connected to {}", peer_id);
            peer->connected = true;
            peer->connected_since = std::chrono::steady_clock::now();
            peer->no_media_warning_logged = false;
            if (session_kind_ == VoiceSessionKind::Direct && active_channel_ == peer_id) {
                set_voice_state_for_session(peer_id, {peer_id});
            }
        } else if (state == rtc::PeerConnection::State::Failed) {
            spdlog::warn("WebRTC connection failed to {}", peer_id);
            peer->connected = false;
            peer->playout_fifo.clear();
            state_.post_ui([this, peer_id]() {
                Message msg;
                msg.type = Message::Type::System;
                msg.sender_id = "voice";
                msg.content = i18n::tr(i18n::I18nKey::VOICE_CONNECTION_FAILED, peer_id);
                msg.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                auto ch = state_.active_channel().value_or("server");
                state_.push_message(ch, std::move(msg));
            });
            if (session_kind_ == VoiceSessionKind::Direct && active_channel_ == peer_id) {
                end_current_session(false);
            } else {
                state_.set_user_voice_status(peer_id, ChannelUserInfo::VoiceStatus::Off);
            }
        } else if (state == rtc::PeerConnection::State::Disconnected) {
            spdlog::warn("WebRTC disconnected from {}", peer_id);
            peer->connected = false;
            peer->playout_fifo.clear();
            if (session_kind_ == VoiceSessionKind::Direct && active_channel_ == peer_id) {
                push_voice_event_to_channel(peer_id, i18n::tr(i18n::I18nKey::CALL_ENDED));
                end_current_session(false);
            } else {
                state_.set_user_voice_status(peer_id, ChannelUserInfo::VoiceStatus::Off);
            }
        }
    });

    peer->pc->onTrack([this, peer_id, peer](std::shared_ptr<rtc::Track> track) {
        peer->recv_track = track;
        peer->recv_track_seen = true;
        spdlog::debug("onTrack fired for {} (room_offer_local={})", peer_id, peer->room_offer_local);
        track->onMessage([this, peer_id, peer](rtc::message_variant msg) {
            if (!std::holds_alternative<rtc::binary>(msg)) return;
            const auto& data = std::get<rtc::binary>(msg);
            if (data.size() <= 12) return;
            ++peer->rx_packets;
            if (peer->rx_packets == 1) {
                spdlog::debug("Received first RTP packet from {}", peer_id);
            }
            uint16_t seq = static_cast<uint16_t>(
                (static_cast<uint16_t>(std::to_integer<uint8_t>(data[2])) << 8) |
                 static_cast<uint16_t>(std::to_integer<uint8_t>(data[3])));
            std::vector<uint8_t> opus;
            opus.reserve(data.size() - 12);
            for (size_t i = 12; i < data.size(); ++i)
                opus.push_back(std::to_integer<uint8_t>(data[i]));
            auto pcm = peer->codec.decode(opus);
            
            // Calculate energy for speaking indicator
            if (!pcm.empty()) {
                float energy = 0.0f;
                for (float s : pcm) energy += s * s;
                peer->last_energy = std::sqrt(energy / pcm.size());
                peer->last_packet_time = std::chrono::steady_clock::now();
                ++peer->decoded_frames;
                if (peer->decoded_frames == 1) {
                    spdlog::debug("Decoded first audio frame from {}", peer_id);
                }
            }
            const bool primed_before = peer->jitter_buf.is_primed();
            peer->jitter_buf.push(seq, std::move(pcm));
            if (!primed_before && peer->jitter_buf.is_primed()) {
                spdlog::debug("Jitter buffer primed for {} (buffered_frames={})",
                              peer_id,
                              peer->jitter_buf.buffered_count());
            }
        });
    });
}

// ── Audio I/O ─────────────────────────────────────────────────────────────────

void VoiceEngine::toggle_voice_mode() {
    voice_mode_ = (voice_mode_ == "ptt") ? "vox" : "ptt";
    VoiceState vs = state_.voice_snapshot();
    vs.voice_mode = voice_mode_;
    state_.set_voice_state(vs); // trigger UI refresh
}

void VoiceEngine::on_capture(const float* pcm, uint32_t frames) {
    if (!pcm || frames == 0) return;

    if (!logged_first_capture_chunk_.exchange(true, std::memory_order_relaxed)) {
        spdlog::debug("Voice capture started (chunk_frames={})", frames);
    }

    if (muted_.load(std::memory_order_relaxed) ||
        !in_voice_.load(std::memory_order_relaxed)) {
        capture_fifo_.clear();
        return;
    }

    // PTT/VOX gate
    if (voice_mode_ == "ptt" && !ptt_active_.load(std::memory_order_relaxed)) {
        capture_fifo_.clear();
        return;
    }

    if (voice_mode_ == "vox") {
        // Simple energy-based VAD
        float energy = 0.0f;
        uint32_t n = std::min(frames, static_cast<uint32_t>(OpusCodec::kFrameSamples));
        for (uint32_t i = 0; i < n; ++i) {
            energy += pcm[i] * pcm[i];
        }
        energy = std::sqrt(energy / n);
        if (energy < cfg_.voice.vad_threshold) {
            capture_fifo_.clear();
            return;
        }
    }

    capture_fifo_.push(pcm, frames);

    while (auto pcm_vec = capture_fifo_.pop_exact(OpusCodec::kFrameSamples)) {
        std::lock_guard lk(mu_);
        for (auto& [pid, peer] : peers_) {
            if (!peer->connected || !peer->send_track) continue;
            auto opus = peer->codec.encode(*pcm_vec);
            if (opus.empty()) continue;

            std::vector<std::byte> rtp(12 + opus.size());
            rtp[0] = std::byte{0x80};
            rtp[1] = std::byte{0x6F};
            rtp[2] = std::byte{static_cast<uint8_t>((rtp_seq_ >> 8) & 0xFF)};
            rtp[3] = std::byte{static_cast<uint8_t>(rtp_seq_ & 0xFF)};
            ++rtp_seq_;
            std::memcpy(rtp.data() + 12, opus.data(), opus.size());
            peer->send_track->send(rtp);
            ++peer->tx_packets;
            if (peer->tx_packets == 1) {
                spdlog::debug("Sent first RTP packet to {}", pid);
            }
        }
    }
}

void VoiceEngine::mix_output(float* out, uint32_t frames) {
    std::fill(out, out + frames, 0.0f);
    if (deafened_.load(std::memory_order_relaxed)) return;

    std::lock_guard lk(mu_);
    for (auto& [pid, peer] : peers_) {
        while (peer->playout_fifo.size() < frames) {
            if (auto frame = peer->jitter_buf.pop()) {
                peer->playout_fifo.push(*frame);
                continue;
            }
            if (peer->connected && peer->rx_packets > 0) {
                peer->playout_fifo.push(peer->codec.decode_plc());
            }
            break;
        }
        peer->playout_fifo.mix_into(out, frames);
    }

    // Soft clip to [-1, 1]
    for (uint32_t i = 0; i < frames; ++i) {
        out[i] = std::max(-1.0f, std::min(1.0f, out[i]));
    }
}

std::vector<std::string> VoiceEngine::get_speaking_peers() const {
    std::vector<std::string> speaking;
    std::lock_guard lk(mu_);
    auto now = std::chrono::steady_clock::now();
    for (const auto& [pid, peer] : peers_) {
        if (!peer->connected) continue;
        // Consider speaking if energy above threshold and packet received recently (< 500ms)
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - peer->last_packet_time).count();
        if (peer->last_energy > 0.01f && elapsed < 500) {
            speaking.push_back(pid);
        }
    }
    return speaking;
}

void VoiceEngine::refresh_speaking_state() {
    if (!in_voice_.load(std::memory_order_relaxed)) return;
    {
        std::lock_guard lk(mu_);
        const auto now = std::chrono::steady_clock::now();
        for (const auto& [peer_id, peer] : peers_) {
            if (!peer->connected || peer->no_media_warning_logged ||
                peer->connected_since == std::chrono::steady_clock::time_point{}) {
                continue;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - peer->connected_since).count();
            if (elapsed < 2000 || peer->rx_packets > 0) {
                continue;
            }
            peer->no_media_warning_logged = true;
            spdlog::warn(
                "WebRTC connected to {} but no media received after {} ms "
                "(recv_track_seen={}, tx_packets={}, rx_packets={}, decoded_frames={}, queued_playout_samples={})",
                peer_id,
                elapsed,
                peer->recv_track_seen,
                peer->tx_packets,
                peer->rx_packets,
                peer->decoded_frames,
                peer->playout_fifo.size());
        }
    }
    auto speaking = get_speaking_peers();
    VoiceState vs = state_.voice_snapshot();
    if (vs.speaking_peers != speaking) {
        vs.speaking_peers = std::move(speaking);
        state_.set_voice_state(vs);
    }
    
    // Update individual user voice status for the user list panel
    // Mark all participants as connected (off) first
    for (const auto& peer_id : vs.participants) {
        bool is_speaking = std::find(speaking.begin(), speaking.end(), peer_id) != speaking.end();
        auto status = is_speaking ? ChannelUserInfo::VoiceStatus::Talking : ChannelUserInfo::VoiceStatus::Off;
        state_.set_user_voice_status(peer_id, status);
    }
}

} // namespace grotto::voice
