#include "voice/voice_engine.hpp"
#include "voice/voice_peer_role.hpp"
#include "i18n/strings.hpp"
#include <rtc/rtcpnackresponder.hpp>
#include <rtc/rtcpsrreporter.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>
#include <variant>

namespace grotto::voice {

namespace {

int clamp_jitter_buffer_frames(int frames) {
    return std::clamp(frames, 2, 10);
}

int64_t steady_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

float clamped_volume_multiplier(int percent) {
    return std::clamp(static_cast<float>(percent), 0.0f, 200.0f) / 100.0f;
}

float ptt_gate_threshold(float vad_threshold) {
    // Keep PTT lighter than VOX so held push-to-talk still captures quiet syllables,
    // but drops low-level breathing and rustle before encoding.
    return std::clamp(vad_threshold * 0.5f, 0.005f, 0.015f);
}

void apply_gain_in_place(std::vector<float>& samples, float gain) {
    if (samples.empty() || gain == 1.0f) {
        return;
    }
    for (float& sample : samples) {
        sample = std::clamp(sample * gain, -1.0f, 1.0f);
    }
}

uint32_t next_voice_ssrc() {
    static std::atomic_uint32_t counter{0x24570000u};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

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

std::string summarize_sdp(std::string_view sdp) {
    std::istringstream in{std::string(sdp)};
    std::string line;
    int audio_m_lines = 0;
    int sendrecv = 0;
    int sendonly = 0;
    int recvonly = 0;
    int inactive = 0;
    std::string mid;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("m=audio ", 0) == 0) ++audio_m_lines;
        else if (line == "a=sendrecv") ++sendrecv;
        else if (line == "a=sendonly") ++sendonly;
        else if (line == "a=recvonly") ++recvonly;
        else if (line == "a=inactive") ++inactive;
        else if (mid.empty() && line.rfind("a=mid:", 0) == 0) mid = line.substr(6);
    }
    std::ostringstream out;
    out << "audio_m_lines=" << audio_m_lines
        << ", mid=" << (mid.empty() ? "<none>" : mid)
        << ", sendrecv=" << sendrecv
        << ", sendonly=" << sendonly
        << ", recvonly=" << recvonly
        << ", inactive=" << inactive;
    return out.str();
}

} // namespace

VoiceEngine::VoiceEngine(AppState& state, const ClientConfig& cfg)
    : state_(state), cfg_(cfg), voice_mode_(normalize_voice_mode(cfg.voice.mode)) {}

VoiceEngine::~VoiceEngine() {
    hangup();
    audio_.close();
}

LocalMonitorSnapshot VoiceEngine::local_monitor_snapshot() const {
    LocalMonitorSnapshot snapshot;
    snapshot.input_rms = local_input_rms_.load(std::memory_order_relaxed);
    snapshot.input_peak = local_input_peak_.load(std::memory_order_relaxed);
    snapshot.noise_suppression_change_ratio =
        local_noise_suppression_change_ratio_.load(std::memory_order_relaxed);
    snapshot.vad_open = local_vad_open_.load(std::memory_order_relaxed);
    snapshot.limiter_active = local_limiter_active_.load(std::memory_order_relaxed);
    snapshot.clipped = local_input_clipped_.load(std::memory_order_relaxed);
    snapshot.noise_suppression_enabled =
        local_noise_suppression_enabled_.load(std::memory_order_relaxed);
    snapshot.noise_suppression_operational =
        local_noise_suppression_operational_.load(std::memory_order_relaxed);
    snapshot.noise_suppression_modified =
        local_noise_suppression_modified_.load(std::memory_order_relaxed);
    std::lock_guard lk(mu_);
    snapshot.loopback_buffer_ms = static_cast<int>(
        (local_test_monitor_fifo_.size() * 1000) / OpusCodec::kSampleRate);
    return snapshot;
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
    reconfigure_noise_suppressor_locked();
    limiter_.configure(cfg_.voice.limiter_enabled, cfg_.voice.limiter_threshold);
    playback_limiter_.configure(cfg_.voice.limiter_enabled, cfg_.voice.limiter_threshold);
    spdlog::info("Voice limiter (enabled={}, threshold={:.2f})",
                 cfg_.voice.limiter_enabled,
                 cfg_.voice.limiter_threshold);

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

void VoiceEngine::reconfigure_noise_suppressor_locked() {
    const bool ns_operational =
        noise_suppressor_.configure(cfg_.voice.noise_suppression_enabled,
                                    cfg_.voice.noise_suppression_level);
    local_noise_suppression_enabled_.store(cfg_.voice.noise_suppression_enabled,
                                           std::memory_order_relaxed);
    local_noise_suppression_operational_.store(ns_operational, std::memory_order_relaxed);
    local_noise_suppression_change_ratio_.store(0.0f, std::memory_order_relaxed);
    local_noise_suppression_modified_.store(false, std::memory_order_relaxed);
    spdlog::info("Noise suppression backend=rnnoise (built_in={}, enabled={}, operational={})",
                 noise_suppressor_.build_available(),
                 cfg_.voice.noise_suppression_enabled,
                 ns_operational);
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
    vs.local_test = false;
    vs.muted = muted_.load(std::memory_order_relaxed);
    vs.deafened = deafened_.load(std::memory_order_relaxed);
    vs.local_capture_active = false;
    vs.active_channel = active_channel;
    vs.participants = participants;
    vs.speaking_peers.clear();
    vs.voice_mode = voice_mode_;
    vs.rtc_connected_peers = 0;
    vs.send_ready_peers = 0;
    vs.recv_ready_peers = 0;
    state_.set_voice_state(std::move(vs));

    for (const auto& participant : participants) {
        state_.set_user_voice_status(participant, ChannelUserInfo::VoiceStatus::Off);
    }
}

void VoiceEngine::set_voice_state_for_local_test() {
    VoiceState vs = state_.voice_snapshot();
    clear_participant_voice_statuses(vs.participants);
    vs.in_voice = true;
    vs.local_test = true;
    vs.muted = muted_.load(std::memory_order_relaxed);
    vs.deafened = deafened_.load(std::memory_order_relaxed);
    vs.local_capture_active = false;
    vs.active_channel.clear();
    vs.participants.clear();
    vs.speaking_peers.clear();
    vs.voice_mode = voice_mode_;
    vs.rtc_connected_peers = 0;
    vs.send_ready_peers = 0;
    vs.recv_ready_peers = 0;
    state_.set_voice_state(std::move(vs));
}

void VoiceEngine::reset_voice_state() {
    VoiceState vs = state_.voice_snapshot();
    clear_participant_voice_statuses(vs.participants);
    vs.in_voice = false;
    vs.local_test = false;
    vs.muted = false;
    vs.deafened = false;
    vs.local_capture_active = false;
    vs.active_channel.clear();
    vs.participants.clear();
    vs.speaking_peers.clear();
    vs.voice_mode = voice_mode_;
    vs.rtc_connected_peers = 0;
    vs.send_ready_peers = 0;
    vs.recv_ready_peers = 0;
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
    {
        std::lock_guard capture_lk(capture_mu_);
        capture_fifo_.clear();
        limiter_.reset();
        playback_limiter_.reset();
        noise_suppressor_.reset();
        vox_gate_.reset();
        ptt_gate_.reset();
        logged_first_capture_chunk_.store(false, std::memory_order_relaxed);
    }

    {
        std::lock_guard lk(mu_);
        peers_.clear();
        local_test_monitor_fifo_.clear();
    }
    local_input_rms_.store(0.0f, std::memory_order_relaxed);
    local_input_peak_.store(0.0f, std::memory_order_relaxed);
    local_vad_open_.store(false, std::memory_order_relaxed);
    local_limiter_active_.store(false, std::memory_order_relaxed);
    local_input_clipped_.store(false, std::memory_order_relaxed);
    local_noise_suppression_change_ratio_.store(0.0f, std::memory_order_relaxed);
    local_noise_suppression_enabled_.store(false, std::memory_order_relaxed);
    local_noise_suppression_operational_.store(false, std::memory_order_relaxed);
    local_noise_suppression_modified_.store(false, std::memory_order_relaxed);

    in_voice_.store(false, std::memory_order_relaxed);
    muted_.store(false, std::memory_order_relaxed);
    deafened_.store(false, std::memory_order_relaxed);
    ptt_active_.store(false, std::memory_order_relaxed);
    last_local_voice_activity_ms_.store(0, std::memory_order_relaxed);
    vox_gate_.reset();
    ptt_gate_.reset();
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

    {
        std::lock_guard capture_lk(capture_mu_);
        capture_fifo_.clear();
        limiter_.reset();
        playback_limiter_.reset();
        noise_suppressor_.reset();
        vox_gate_.reset();
        ptt_gate_.reset();
        logged_first_capture_chunk_.store(false, std::memory_order_relaxed);
    }

    if (!open_audio_or_report("voice room")) {
        in_voice_.store(false, std::memory_order_relaxed);
        session_kind_ = VoiceSessionKind::None;
        active_channel_.clear();
        return;
    }

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
    vox_gate_.reset();
    ptt_gate_.reset();

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
    vox_gate_.reset();
    ptt_gate_.reset();

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

bool VoiceEngine::start_local_test() {
    if (in_voice_.load(std::memory_order_relaxed)) {
        if (session_kind_ == VoiceSessionKind::Direct) {
            end_current_session(true, active_channel_);
        } else {
            end_current_session(false);
        }
    }

    in_voice_.store(true, std::memory_order_relaxed);
    active_channel_.clear();
    muted_.store(false, std::memory_order_relaxed);
    deafened_.store(false, std::memory_order_relaxed);
    ptt_active_.store(false, std::memory_order_relaxed);
    session_kind_ = VoiceSessionKind::LocalTest;

    {
        std::lock_guard capture_lk(capture_mu_);
        capture_fifo_.clear();
        limiter_.reset();
        playback_limiter_.reset();
        noise_suppressor_.reset();
        vox_gate_.reset();
        ptt_gate_.reset();
        logged_first_capture_chunk_.store(false, std::memory_order_relaxed);
    }
    {
        std::lock_guard lk(mu_);
        local_test_monitor_fifo_.clear();
    }
    local_input_rms_.store(0.0f, std::memory_order_relaxed);
    local_input_peak_.store(0.0f, std::memory_order_relaxed);
    local_vad_open_.store(false, std::memory_order_relaxed);
    local_limiter_active_.store(false, std::memory_order_relaxed);
    local_input_clipped_.store(false, std::memory_order_relaxed);
    local_noise_suppression_change_ratio_.store(0.0f, std::memory_order_relaxed);
    local_noise_suppression_enabled_.store(false, std::memory_order_relaxed);
    local_noise_suppression_operational_.store(false, std::memory_order_relaxed);
    local_noise_suppression_modified_.store(false, std::memory_order_relaxed);

    if (!open_audio_or_report("local voice test")) {
        in_voice_.store(false, std::memory_order_relaxed);
        session_kind_ = VoiceSessionKind::None;
        return false;
    }

    set_voice_state_for_local_test();
    spdlog::info("Started local voice self-test");
    return true;
}

void VoiceEngine::stop_local_test() {
    if (!in_voice_.load(std::memory_order_relaxed) ||
        session_kind_ != VoiceSessionKind::LocalTest) {
        return;
    }
    end_current_session(false);
}

// ── Controls ─────────────────────────────────────────────────────────────────

void VoiceEngine::set_muted(bool muted) {
    muted_.store(muted, std::memory_order_relaxed);
    if (muted) {
        vox_gate_.reset();
        ptt_gate_.reset();
        last_local_voice_activity_ms_.store(0, std::memory_order_relaxed);
    }
    VoiceState vs = state_.voice_snapshot();
    vs.muted = muted;
    if (muted) {
        vs.local_capture_active = false;
    }
    state_.set_voice_state(vs);
}

void VoiceEngine::set_deafened(bool deafened) {
    deafened_.store(deafened, std::memory_order_relaxed);
    VoiceState vs = state_.voice_snapshot();
    vs.deafened = deafened;
    state_.set_voice_state(vs);
}

void VoiceEngine::set_ptt_active(bool active) {
    const bool previous = ptt_active_.exchange(active, std::memory_order_relaxed);
    if (previous == active) {
        return;
    }
    if (!is_vox_mode(voice_mode_) && in_voice_.load(std::memory_order_relaxed)) {
        std::lock_guard capture_lk(capture_mu_);
        noise_suppressor_.clear_pending();
        capture_fifo_.clear();
        reconfigure_noise_suppressor_locked();
    }
    if (!active) {
        vox_gate_.reset();
        ptt_gate_.reset();
        last_local_voice_activity_ms_.store(0, std::memory_order_relaxed);
    }
    if (!is_vox_mode(voice_mode_)) {
        VoiceState vs = state_.voice_snapshot();
        vs.local_capture_active =
            in_voice_.load(std::memory_order_relaxed) &&
            !muted_.load(std::memory_order_relaxed) &&
            active;
        state_.set_voice_state(vs);
    }
    spdlog::debug("PTT {} (mode={}, in_voice={})",
                  active ? "enabled" : "disabled",
                  voice_mode_,
                  in_voice_.load(std::memory_order_relaxed));
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
        spdlog::debug("Remote OFFER SDP from {}: {}", from, summarize_sdp(sdp));
        auto peer = get_or_create_peer(from, false);
        if (peer && peer->pc) {
            try {
                ensure_send_track(peer);
                peer->pc->setRemoteDescription(
                    rtc::Description(sdp, rtc::Description::Type::Offer));
            } catch (const std::exception& e) {
                spdlog::debug("Ignoring OFFER from {} in current signaling state: {}",
                              from, e.what());
            }
        }
        break;
    }

    case VoiceSignal::ANSWER: {
        spdlog::debug("Received ANSWER from {}", from);
        spdlog::debug("Remote ANSWER SDP from {}: {}", from, summarize_sdp(sdp));
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

    auto peer = std::make_shared<PeerConn>(clamp_jitter_buffer_frames(cfg_.voice.jitter_buffer_frames));
    peer->peer_id = peer_id;
    peer->pc      = std::make_shared<rtc::PeerConnection>(make_rtc_config());
    peer->send_ssrc = next_voice_ssrc();
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
    desc.addSSRC(peer->send_ssrc,
                 "grotto-" + state_.local_user_id(),
                 state_.local_user_id(),
                 "audio");
    peer->send_track = peer->pc->addTrack(std::move(desc));
    auto rtp_config = std::make_shared<rtc::RtpPacketizationConfig>(
        peer->send_ssrc,
        "grotto-" + state_.local_user_id(),
        111,
        OpusCodec::kSampleRate);
    rtp_config->mid = std::string("audio");
    auto packetizer = std::make_shared<rtc::OpusRtpPacketizer>(rtp_config);
    packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(rtp_config));
    packetizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());
    peer->send_track->setMediaHandler(packetizer);
    const auto peer_id = peer->peer_id;
    peer->send_track->onOpen([this, peer, peer_id]() {
        std::lock_guard lk(mu_);
        spdlog::debug("Send track open for {}", peer_id);
    });
    peer->send_track->onClosed([this, peer, peer_id]() {
        std::lock_guard lk(mu_);
        spdlog::debug("Send track closed for {}", peer_id);
    });
    peer->send_track->onError([this, peer, peer_id](std::string error) {
        std::lock_guard lk(mu_);
        spdlog::warn("Send track error for {}: {}", peer_id, error);
    });
    spdlog::debug("Created send track for {} (isOpen={})", peer_id, peer->send_track->isOpen());
    attach_receive_handler(peer, peer->send_track, "send_track");
}

void VoiceEngine::attach_receive_handler(const std::shared_ptr<PeerConn>& peer,
                                         const std::shared_ptr<rtc::Track>& track,
                                         const char* source_label) {
    if (!peer || !track) {
        return;
    }

    const auto peer_id = peer->peer_id;
    track->onMessage([this, peer_id, peer, source_label](rtc::message_variant msg) {
        if (!std::holds_alternative<rtc::binary>(msg)) return;
        const auto& data = std::get<rtc::binary>(msg);
        if (data.size() < 12) return;

        const uint8_t b0 = std::to_integer<uint8_t>(data[0]);
        const uint8_t b1 = std::to_integer<uint8_t>(data[1]);
        const uint8_t version = (b0 >> 6) & 0x03;
        if (version != 2) {
            return;
        }

        const uint8_t payload_type = b1 & 0x7F;
        if (payload_type >= 64 && payload_type <= 95) {
            return; // RTCP / reserved control payloads, not Opus media.
        }

        const size_t csrc_count = b0 & 0x0F;
        const bool has_extension = (b0 & 0x10) != 0;
        const bool has_padding = (b0 & 0x20) != 0;
        size_t header_size = 12 + (csrc_count * 4);
        if (data.size() < header_size) {
            return;
        }
        if (has_extension) {
            if (data.size() < header_size + 4) {
                return;
            }
            const size_t extension_words =
                (static_cast<size_t>(std::to_integer<uint8_t>(data[header_size + 2])) << 8) |
                 static_cast<size_t>(std::to_integer<uint8_t>(data[header_size + 3]));
            header_size += 4 + (extension_words * 4);
            if (data.size() < header_size) {
                return;
            }
        }

        size_t payload_end = data.size();
        if (has_padding) {
            const uint8_t padding_size = std::to_integer<uint8_t>(data.back());
            if (padding_size == 0 || padding_size > payload_end - header_size) {
                return;
            }
            payload_end -= padding_size;
        }
        if (payload_end <= header_size) {
            return;
        }

        uint16_t seq = static_cast<uint16_t>(
            (static_cast<uint16_t>(std::to_integer<uint8_t>(data[2])) << 8) |
             static_cast<uint16_t>(std::to_integer<uint8_t>(data[3])));
        std::vector<uint8_t> opus;
        opus.reserve(payload_end - header_size);
        for (size_t i = header_size; i < payload_end; ++i) {
            opus.push_back(std::to_integer<uint8_t>(data[i]));
        }
        if (opus.empty()) {
            return;
        }

        auto pcm = peer->codec.decode(opus);
        if (!pcm.empty()) {
            float energy = 0.0f;
            for (float s : pcm) energy += s * s;
            const float rms = std::sqrt(energy / pcm.size());
            std::lock_guard lk(mu_);
            ++peer->rx_packets;
            peer->recv_track_seen = true;
            if (peer->rx_packets == 1) {
                spdlog::debug("Received first RTP packet from {} via {}", peer_id, source_label);
            }
            peer->last_energy = rms;
            peer->last_packet_time = std::chrono::steady_clock::now();
            ++peer->decoded_frames;
            if (peer->decoded_frames == 1) {
                spdlog::debug("Decoded first audio frame from {} via {}", peer_id, source_label);
            }
        } else {
            std::lock_guard lk(mu_);
            ++peer->rx_packets;
            peer->recv_track_seen = true;
            if (peer->rx_packets == 1) {
                spdlog::debug("Received first RTP packet from {} via {}", peer_id, source_label);
            }
        }
        std::lock_guard lk(mu_);
        const bool primed_before = peer->jitter_buf.is_primed();
        peer->jitter_buf.push(seq, std::move(pcm));
        if (!primed_before && peer->jitter_buf.is_primed()) {
            spdlog::debug("Jitter buffer primed for {} (buffered_frames={})",
                          peer_id,
                          peer->jitter_buf.buffered_count());
        }
    });
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
        spdlog::debug("Local {} SDP for {}: {}",
                      desc.type() == rtc::Description::Type::Offer ? "OFFER" : "ANSWER",
                      peer_id,
                      summarize_sdp(std::string(desc)));
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
            spdlog::info("WebRTC connected to {} (has_media={}, bytes_sent={}, bytes_received={})",
                         peer_id,
                         peer->pc->hasMedia(),
                         peer->pc->bytesSent(),
                         peer->pc->bytesReceived());
            {
                std::lock_guard lk(mu_);
                peer->connected = true;
                peer->connected_since = std::chrono::steady_clock::now();
                peer->no_media_warning_logged = false;
            }
            if (session_kind_ == VoiceSessionKind::Direct && active_channel_ == peer_id) {
                set_voice_state_for_session(peer_id, {peer_id});
            }
        } else if (state == rtc::PeerConnection::State::Failed) {
            spdlog::warn("WebRTC connection failed to {}", peer_id);
            {
                std::lock_guard lk(mu_);
                peer->connected = false;
                peer->playout_fifo.clear();
            }
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
            {
                std::lock_guard lk(mu_);
                peer->connected = false;
                peer->playout_fifo.clear();
            }
            if (session_kind_ == VoiceSessionKind::Direct && active_channel_ == peer_id) {
                push_voice_event_to_channel(peer_id, i18n::tr(i18n::I18nKey::CALL_ENDED));
                end_current_session(false);
            } else {
                state_.set_user_voice_status(peer_id, ChannelUserInfo::VoiceStatus::Off);
            }
        }
    });

    peer->pc->onSignalingStateChange([peer_id](rtc::PeerConnection::SignalingState state) {
        spdlog::debug("WebRTC signaling state for {} -> {}", peer_id, static_cast<int>(state));
    });

    peer->pc->onTrack([this, peer_id, peer](std::shared_ptr<rtc::Track> track) {
        {
            std::lock_guard lk(mu_);
            peer->recv_track = track;
            peer->recv_track_seen = true;
        }
        spdlog::debug("onTrack fired for {} (room_offer_local={})", peer_id, peer->room_offer_local);
        attach_receive_handler(peer, track, "onTrack");
    });
}

// ── Audio I/O ─────────────────────────────────────────────────────────────────

void VoiceEngine::toggle_voice_mode() {
    set_voice_mode(next_voice_mode(voice_mode_));
}

void VoiceEngine::set_voice_mode(std::string mode) {
    voice_mode_ = normalize_voice_mode(std::move(mode));
    vox_gate_.reset();
    ptt_gate_.reset();
    if (is_vox_mode(voice_mode_)) {
        ptt_active_.store(false, std::memory_order_relaxed);
    }
    last_local_voice_activity_ms_.store(0, std::memory_order_relaxed);
    VoiceState vs = state_.voice_snapshot();
    vs.voice_mode = voice_mode_;
    vs.local_capture_active = false;
    state_.set_voice_state(vs); // trigger UI refresh
}

void VoiceEngine::on_capture(const float* pcm, uint32_t frames) {
    if (!pcm || frames == 0) return;

    std::lock_guard capture_lk(capture_mu_);

    if (!logged_first_capture_chunk_.exchange(true, std::memory_order_relaxed)) {
        spdlog::debug("Voice capture started (chunk_frames={})", frames);
    }

    if (muted_.load(std::memory_order_relaxed) ||
        !in_voice_.load(std::memory_order_relaxed)) {
        vox_gate_.reset();
        ptt_gate_.reset();
        noise_suppressor_.clear_pending();
        capture_fifo_.clear();
        local_input_rms_.store(0.0f, std::memory_order_relaxed);
        local_input_peak_.store(0.0f, std::memory_order_relaxed);
        local_vad_open_.store(false, std::memory_order_relaxed);
        local_limiter_active_.store(false, std::memory_order_relaxed);
        local_input_clipped_.store(false, std::memory_order_relaxed);
        local_noise_suppression_change_ratio_.store(0.0f, std::memory_order_relaxed);
        local_noise_suppression_modified_.store(false, std::memory_order_relaxed);
        std::lock_guard lk(mu_);
        local_test_monitor_fifo_.clear();
        return;
    }

    // Talk-key/VOX gate
    if (!is_vox_mode(voice_mode_) && !ptt_active_.load(std::memory_order_relaxed)) {
        vox_gate_.reset();
        ptt_gate_.reset();
        noise_suppressor_.clear_pending();
        capture_fifo_.clear();
        local_input_rms_.store(0.0f, std::memory_order_relaxed);
        local_input_peak_.store(0.0f, std::memory_order_relaxed);
        local_vad_open_.store(false, std::memory_order_relaxed);
        local_limiter_active_.store(false, std::memory_order_relaxed);
        local_input_clipped_.store(false, std::memory_order_relaxed);
        local_noise_suppression_change_ratio_.store(0.0f, std::memory_order_relaxed);
        local_noise_suppression_modified_.store(false, std::memory_order_relaxed);
        std::lock_guard lk(mu_);
        local_test_monitor_fifo_.clear();
        return;
    }

    std::vector<float> capture_chunk;
    const float input_gain = clamped_volume_multiplier(cfg_.voice.input_volume);
    const float* capture_ptr = pcm;
    if (input_gain != 1.0f) {
        capture_chunk.assign(pcm, pcm + frames);
        apply_gain_in_place(capture_chunk, input_gain);
        capture_ptr = capture_chunk.data();
    }

    for (auto& processed_chunk : noise_suppressor_.process_capture_chunk(capture_ptr, frames)) {
        local_noise_suppression_change_ratio_.store(
            noise_suppressor_.last_frame_change_ratio(),
            std::memory_order_relaxed);
        local_noise_suppression_modified_.store(
            noise_suppressor_.last_frame_modified(),
            std::memory_order_relaxed);
        float chunk_energy = 0.0f;
        float chunk_peak = 0.0f;
        for (float sample : processed_chunk) {
            chunk_energy += sample * sample;
            chunk_peak = std::max(chunk_peak, std::abs(sample));
        }
        const float chunk_rms = processed_chunk.empty()
            ? 0.0f
            : std::sqrt(chunk_energy / static_cast<float>(processed_chunk.size()));
        local_input_rms_.store(chunk_rms, std::memory_order_relaxed);
        local_input_peak_.store(chunk_peak, std::memory_order_relaxed);
        local_input_clipped_.store(chunk_peak >= 0.995f, std::memory_order_relaxed);

        if (is_vox_mode(voice_mode_)) {
            const int64_t now_ms = steady_now_ms();
            const auto vad_decision =
                vox_gate_.update(chunk_rms, cfg_.voice.vad_threshold, now_ms);
            if (!vad_decision.gate_open) {
                local_vad_open_.store(false, std::memory_order_relaxed);
                capture_fifo_.clear();
                noise_suppressor_.clear_pending();
                local_limiter_active_.store(false, std::memory_order_relaxed);
                local_noise_suppression_change_ratio_.store(0.0f, std::memory_order_relaxed);
                local_noise_suppression_modified_.store(false, std::memory_order_relaxed);
                std::lock_guard lk(mu_);
                local_test_monitor_fifo_.clear();
                continue;
            }
            local_vad_open_.store(true, std::memory_order_relaxed);
            if (vad_decision.signal_detected) {
                last_local_voice_activity_ms_.store(now_ms, std::memory_order_relaxed);
            }
        } else {
            const auto ptt_decision =
                ptt_gate_.update(chunk_rms,
                                 ptt_gate_threshold(cfg_.voice.vad_threshold),
                                 steady_now_ms());
            if (!ptt_decision.gate_open) {
                local_vad_open_.store(false, std::memory_order_relaxed);
                capture_fifo_.clear();
                local_limiter_active_.store(false, std::memory_order_relaxed);
                std::lock_guard lk(mu_);
                local_test_monitor_fifo_.clear();
                continue;
            }
            local_vad_open_.store(true, std::memory_order_relaxed);
        }

        capture_fifo_.push(processed_chunk);
    }

    while (auto pcm_vec = capture_fifo_.pop_exact(OpusCodec::kFrameSamples)) {
        float pre_limiter_peak = 0.0f;
        for (float sample : *pcm_vec) {
            pre_limiter_peak = std::max(pre_limiter_peak, std::abs(sample));
        }
        limiter_.process(*pcm_vec);
        local_limiter_active_.store(
            cfg_.voice.limiter_enabled &&
            pre_limiter_peak > (std::clamp(cfg_.voice.limiter_threshold, 0.20f, 0.99f) * 0.98f),
            std::memory_order_relaxed);
        float rms = 0.0f;
        for (float sample : *pcm_vec) {
            rms += sample * sample;
        }
        rms = std::sqrt(rms / static_cast<float>(pcm_vec->size()));

        if (session_kind_ == VoiceSessionKind::LocalTest) {
            std::lock_guard lk(mu_);
            local_test_monitor_fifo_.push(*pcm_vec);
            const size_t max_buffered_samples = static_cast<size_t>(OpusCodec::kFrameSamples * 4);
            if (local_test_monitor_fifo_.size() > max_buffered_samples) {
                local_test_monitor_fifo_.discard_front(local_test_monitor_fifo_.size() - max_buffered_samples);
            }
            continue;
        }

        std::lock_guard lk(mu_);
        for (auto& [pid, peer] : peers_) {
            if (!peer->connected || !peer->send_track) continue;
            peer->local_capture_rms = rms;
            auto opus = peer->codec.encode(*pcm_vec);
            if (opus.empty()) {
                spdlog::debug("Dropped encoded frame for {} (rms={:.5f}, track_is_open={})",
                              pid,
                              rms,
                              peer->send_track->isOpen());
                continue;
            }
            if (!peer->send_track->isOpen()) {
                if (!peer->send_blocked_logged) {
                    peer->send_blocked_logged = true;
                    spdlog::debug("Deferring audio frame to {} until track opens (rms={:.5f}, opus_bytes={})",
                                  pid,
                                  rms,
                                  opus.size());
                }
                continue;
            }
            if (peer->tx_packets == 0) {
                spdlog::debug("Attempting first audio frame to {} (rms={:.5f}, opus_bytes={}, track_is_open={})",
                              pid,
                              rms,
                              opus.size(),
                              peer->send_track->isOpen());
            }
            try {
                peer->send_track->sendFrame(
                    reinterpret_cast<const rtc::byte*>(opus.data()),
                    opus.size(),
                    rtc::FrameInfo{std::chrono::duration<double, std::milli>(
                        static_cast<double>(peer->tx_packets * OpusCodec::kFrameMs))});
            } catch (const std::exception& e) {
                spdlog::warn("Track sendFrame failed for {}: {}", pid, e.what());
                continue;
            }
            peer->send_blocked_logged = false;
            ++peer->tx_packets;
            if (peer->tx_packets == 1) {
                spdlog::debug("Sent first audio frame to {}", pid);
            }
        }
    }
}

void VoiceEngine::mix_output(float* out, uint32_t frames) {
    std::fill(out, out + frames, 0.0f);
    if (deafened_.load(std::memory_order_relaxed)) return;

    std::lock_guard lk(mu_);
    if (session_kind_ == VoiceSessionKind::LocalTest) {
        const size_t target_buffered_samples =
            std::max<size_t>(frames, static_cast<size_t>(OpusCodec::kFrameSamples));
        const size_t max_buffered_samples =
            target_buffered_samples + static_cast<size_t>(OpusCodec::kFrameSamples);
        if (local_test_monitor_fifo_.size() > max_buffered_samples) {
            local_test_monitor_fifo_.discard_front(local_test_monitor_fifo_.size() - target_buffered_samples);
        }
        local_test_monitor_fifo_.mix_into(out, frames);
    }
    for (auto& [pid, peer] : peers_) {
        const size_t target_buffered_samples =
            std::max<size_t>(frames, static_cast<size_t>(OpusCodec::kFrameSamples * 2));
        const size_t max_buffered_samples = target_buffered_samples + static_cast<size_t>(OpusCodec::kFrameSamples);
        while (peer->playout_fifo.size() < target_buffered_samples) {
            if (auto frame = peer->jitter_buf.pop()) {
                peer->playout_fifo.push(*frame);
                continue;
            }
            if (peer->playout_fifo.size() >= frames) {
                break;
            }
            const int buffered_jitter_frames = peer->jitter_buf.buffered_count();
            if (buffered_jitter_frames >= clamp_jitter_buffer_frames(cfg_.voice.jitter_buffer_frames) &&
                peer->jitter_buf.resync_to_oldest()) {
                spdlog::debug("Resynced jitter buffer for {} after underrun (buffered_frames={})",
                              pid,
                              buffered_jitter_frames);
                if (auto frame = peer->jitter_buf.pop()) {
                    peer->playout_fifo.push(*frame);
                    continue;
                }
            }
            if (peer->connected && peer->rx_packets > 0) {
                peer->playout_fifo.push(peer->codec.decode_plc());
            }
            break;
        }
        if (peer->playout_fifo.size() > max_buffered_samples) {
            const size_t discard_samples = peer->playout_fifo.size() - target_buffered_samples;
            peer->playout_fifo.discard_front(discard_samples);
            spdlog::debug("Trimmed playout drift for {} by {} samples (remaining={})",
                          pid,
                          discard_samples,
                          peer->playout_fifo.size());
        }
        peer->playout_fifo.mix_into(out, frames);
    }

    std::vector<float> mixed_frame(out, out + frames);
    const float output_gain = clamped_volume_multiplier(cfg_.voice.output_volume);
    apply_gain_in_place(mixed_frame, output_gain);
    playback_limiter_.process(mixed_frame);

    // Soft clip to [-1, 1]
    for (uint32_t i = 0; i < frames; ++i) {
        out[i] = std::max(-1.0f, std::min(1.0f, mixed_frame[i]));
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
    std::size_t rtc_connected_peers = 0;
    std::size_t send_ready_peers = 0;
    std::size_t recv_ready_peers = 0;
    {
        std::lock_guard lk(mu_);
        const auto now = std::chrono::steady_clock::now();
        for (const auto& [peer_id, peer] : peers_) {
            if (peer->connected) {
                ++rtc_connected_peers;
            }
            if (peer->send_track && peer->tx_packets > 0) {
                ++send_ready_peers;
            }
            if (peer->recv_track_seen || peer->rx_packets > 0) {
                ++recv_ready_peers;
            }
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
                "(recv_track_seen={}, send_attempted={}, last_capture_rms={:.5f}, tx_packets={}, rx_packets={}, decoded_frames={}, queued_playout_samples={}, has_media={}, bytes_sent={}, bytes_received={}, signaling_state={})",
                peer_id,
                elapsed,
                peer->recv_track_seen,
                peer->tx_packets > 0,
                peer->local_capture_rms,
                peer->tx_packets,
                peer->rx_packets,
                peer->decoded_frames,
                peer->playout_fifo.size(),
                peer->pc ? peer->pc->hasMedia() : false,
                peer->pc ? peer->pc->bytesSent() : 0,
                peer->pc ? peer->pc->bytesReceived() : 0,
                peer->pc ? static_cast<int>(peer->pc->signalingState()) : -1);
        }
    }
    auto speaking = get_speaking_peers();
    const int64_t now_ms = steady_now_ms();
    const bool local_capture_active =
        !muted_.load(std::memory_order_relaxed) &&
        ((!is_vox_mode(voice_mode_) && ptt_active_.load(std::memory_order_relaxed)) ||
         (is_vox_mode(voice_mode_) &&
          (now_ms - last_local_voice_activity_ms_.load(std::memory_order_relaxed)) < 400));
    VoiceState vs = state_.voice_snapshot();
    if (vs.speaking_peers != speaking ||
        vs.local_capture_active != local_capture_active ||
        vs.rtc_connected_peers != rtc_connected_peers ||
        vs.send_ready_peers != send_ready_peers ||
        vs.recv_ready_peers != recv_ready_peers) {
        vs.speaking_peers = std::move(speaking);
        vs.local_capture_active = local_capture_active;
        vs.rtc_connected_peers = rtc_connected_peers;
        vs.send_ready_peers = send_ready_peers;
        vs.recv_ready_peers = recv_ready_peers;
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
