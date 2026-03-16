#include "voice_engine.hpp"
#include <android/log.h>
#include <cstring>
#include <chrono>

#define LOG_TAG "VoiceEngine"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace grotto::voice {

// ============================================================================
// AudioDevice - Stub implementation (to be implemented with Oboe)
// ============================================================================

class AudioDevice {
public:
    AudioDevice(int sample_rate, int frames_per_buffer) 
        : sample_rate_(sample_rate), frames_per_buffer_(frames_per_buffer) {}
    
    bool init() {
        LOGI("AudioDevice init (stub) - sample_rate=%d, frames=%d", 
             sample_rate_, frames_per_buffer_);
        // TODO: Initialize Oboe audio stream
        return true;
    }
    
    void start() {
        LOGI("AudioDevice start (stub)");
        running_ = true;
        // TODO: Start Oboe streams
    }
    
    void stop() {
        LOGI("AudioDevice stop (stub)");
        running_ = false;
        // TODO: Stop Oboe streams
    }
    
    void destroy() {
        LOGI("AudioDevice destroy (stub)");
        stop();
        // TODO: Cleanup Oboe
    }
    
    void setMuted(bool muted) { muted_ = muted; }
    void setDeafened(bool deafened) { deafened_ = deafened; }
    
    // Called from audio callback to get captured audio
    void onAudioCaptured(const int16_t* data, size_t frames) {
        if (muted_ || !running_) return;
        // TODO: Pass to Opus encoder and send to peers
        (void)data;
        (void)frames;
    }
    
    // Called to play received audio
    void onAudioReceived(const std::string& peer_id, const int16_t* data, size_t frames) {
        if (deafened_ || !running_) return;
        // TODO: Mix and play audio
        (void)peer_id;
        (void)data;
        (void)frames;
    }

private:
    int sample_rate_;
    int frames_per_buffer_;
    bool running_ = false;
    bool muted_ = false;
    bool deafened_ = false;
};

// ============================================================================
// PeerConnection - Stub implementation (to be implemented with libdatachannel)
// ============================================================================

class PeerConnection {
public:
    PeerConnection(const std::string& peer_id, VoiceEngine* engine)
        : peer_id_(peer_id), engine_(engine) {}
    
    bool init() {
        LOGI("PeerConnection init (stub) for %s", peer_id_.c_str());
        // TODO: Initialize libdatachannel peer connection
        return true;
    }
    
    void createOffer() {
        LOGI("createOffer (stub) for %s", peer_id_.c_str());
        // TODO: Create WebRTC offer
    }
    
    void handleOffer(const std::vector<uint8_t>& data) {
        LOGI("handleOffer (stub) from %s, size=%zu", peer_id_.c_str(), data.size());
        // TODO: Set remote description and create answer
        (void)data;
    }
    
    void handleAnswer(const std::vector<uint8_t>& data) {
        LOGI("handleAnswer (stub) from %s, size=%zu", peer_id_.c_str(), data.size());
        // TODO: Set remote description
        (void)data;
    }
    
    void addIceCandidate(const std::vector<uint8_t>& data) {
        LOGI("addIceCandidate (stub) from %s, size=%zu", peer_id_.c_str(), data.size());
        // TODO: Add ICE candidate
        (void)data;
    }
    
    void close() {
        LOGI("PeerConnection close (stub) for %s", peer_id_.c_str());
        // TODO: Close peer connection
    }
    
    void sendAudio(const std::vector<uint8_t>& opus_data) {
        // TODO: Send audio packet via data channel
        (void)opus_data;
    }
    
    const std::string& peerId() const { return peer_id_; }
    bool isConnected() const { return connected_; }

private:
    std::string peer_id_;
    VoiceEngine* engine_;
    bool connected_ = false;
};

// ============================================================================
// VoiceEngine Implementation
// ============================================================================

VoiceEngine::VoiceEngine() = default;

VoiceEngine::~VoiceEngine() {
    destroy();
}

bool VoiceEngine::init(int sample_rate, int frames_per_buffer) {
    if (initialized_) {
        LOGW("VoiceEngine already initialized");
        return true;
    }
    
    sample_rate_ = sample_rate;
    frames_per_buffer_ = frames_per_buffer;
    
    LOGI("Initializing VoiceEngine: sample_rate=%d, frames=%d", 
         sample_rate_, frames_per_buffer_);
    
    // Initialize audio device
    audio_device_ = std::make_unique<AudioDevice>(sample_rate_, frames_per_buffer_);
    if (!audio_device_->init()) {
        LOGE("Failed to initialize audio device");
        return false;
    }
    
    initialized_ = true;
    LOGI("VoiceEngine initialized successfully");
    return true;
}

void VoiceEngine::destroy() {
    if (!initialized_) return;
    
    LOGI("Destroying VoiceEngine");
    
    leaveRoom();
    
    if (audio_device_) {
        audio_device_->destroy();
        audio_device_.reset();
    }
    
    initialized_ = false;
    LOGI("VoiceEngine destroyed");
}

void VoiceEngine::joinRoom(const std::string& channel_id, bool is_private_call) {
    if (!initialized_) {
        LOGE("VoiceEngine not initialized");
        return;
    }
    
    if (isInRoom()) {
        LOGW("Already in a room, leaving first");
        leaveRoom();
    }
    
    current_channel_id_ = channel_id;
    is_private_call_ = is_private_call;
    
    LOGI("Joining room: %s (private=%d)", channel_id.c_str(), is_private_call);
    
    setConnectionState(ConnectionState::CONNECTING);
    
    // Start audio device
    audio_device_->start();
    
    // For group voice, we'd connect to a SFU/MCU or mesh with all peers
    // For private call, we'll wait for the call to be initiated
    if (!is_private_call) {
        // Group voice - in real implementation, we'd get peer list from server
        setConnectionState(ConnectionState::CONNECTED);
    }
}

void VoiceEngine::leaveRoom() {
    if (!isInRoom()) return;
    
    LOGI("Leaving room: %s", current_channel_id_.c_str());
    
    // Close all peer connections
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        for (auto& [id, peer] : peers_) {
            if (peer) peer->close();
            if (peer_left_cb_) peer_left_cb_(id);
        }
        peers_.clear();
    }
    
    // Stop audio
    if (audio_device_) {
        audio_device_->stop();
    }
    
    // Send BYE signal if connected
    if (connection_state_ == ConnectionState::CONNECTED && !current_peer_id_.empty()) {
        // In real implementation, send BYE via signaling
        LOGI("Sending BYE to %s", current_peer_id_.c_str());
    }
    
    setConnectionState(ConnectionState::DISCONNECTED);
    setCallState(CallState::IDLE);
    
    current_channel_id_.clear();
    current_peer_id_.clear();
    is_private_call_ = false;
}

void VoiceEngine::call(const std::string& peer_id) {
    if (!initialized_) {
        LOGE("VoiceEngine not initialized");
        return;
    }
    
    if (isInRoom()) {
        LOGW("Already in a room/call");
        return;
    }
    
    LOGI("Calling peer: %s", peer_id.c_str());
    
    current_peer_id_ = peer_id;
    is_private_call_ = true;
    setCallState(CallState::CALLING);
    setConnectionState(ConnectionState::CONNECTING);
    
    // Create peer connection
    auto peer = std::make_unique<PeerConnection>(peer_id, this);
    if (!peer->init()) {
        LOGE("Failed to create peer connection");
        setCallState(CallState::IDLE);
        setConnectionState(ConnectionState::FAILED);
        return;
    }
    
    // Store peer
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        peers_[peer_id] = std::move(peer);
    }
    
    // Start audio
    audio_device_->start();
    
    // Create and send offer
    createOffer(peer_id);
}

void VoiceEngine::acceptCall() {
    if (call_state_ != CallState::RINGING) {
        LOGW("Not in ringing state");
        return;
    }
    
    LOGI("Accepting call from %s", current_peer_id_.c_str());
    
    setCallState(CallState::CONNECTED);
    setConnectionState(ConnectionState::CONNECTED);
    
    // Start audio
    audio_device_->start();
    
    if (call_accepted_cb_) {
        call_accepted_cb_(current_peer_id_);
    }
}

void VoiceEngine::declineCall(const std::string& reason) {
    if (call_state_ != CallState::RINGING) {
        LOGW("Not in ringing state");
        return;
    }
    
    LOGI("Declining call from %s: %s", current_peer_id_.c_str(), reason.c_str());
    
    // Send decline signal
    // In real implementation, send via signaling server
    
    setCallState(CallState::IDLE);
    current_peer_id_.clear();
    
    if (call_declined_cb_) {
        call_declined_cb_(current_peer_id_, reason);
    }
}

void VoiceEngine::hangup() {
    if (call_state_ == CallState::IDLE) {
        LOGW("Not in a call");
        return;
    }
    
    LOGI("Hanging up");
    
    leaveRoom();
}

void VoiceEngine::setMuted(bool muted) {
    muted_ = muted;
    if (audio_device_) {
        audio_device_->setMuted(muted);
    }
    LOGI("Muted: %d", muted);
}

void VoiceEngine::setDeafened(bool deafened) {
    deafened_ = deafened;
    if (audio_device_) {
        audio_device_->setDeafened(deafened);
    }
    LOGI("Deafened: %d", deafened);
}

void VoiceEngine::onSignal(const std::string& from_user, SignalType type, 
                           const std::vector<uint8_t>& data) {
    if (!initialized_) {
        LOGE("VoiceEngine not initialized");
        return;
    }
    
    LOGD("Received signal from %s, type=%d, size=%zu", 
         from_user.c_str(), static_cast<int>(type), data.size());
    
    switch (type) {
        case SignalType::OFFER:
            handleOffer(from_user, data);
            break;
        case SignalType::ANSWER:
            handleAnswer(from_user, data);
            break;
        case SignalType::ICE_CANDIDATE:
            handleIceCandidate(from_user, data);
            break;
        case SignalType::BYE:
            handleBye(from_user);
            break;
        case SignalType::CALL_REQUEST:
            handleCallRequest(from_user, data);
            break;
        case SignalType::CALL_ACCEPT:
            handleCallAccept(from_user);
            break;
        case SignalType::CALL_DECLINE:
            handleCallDecline(from_user, data);
            break;
        default:
            LOGW("Unknown signal type: %d", static_cast<int>(type));
    }
}

void VoiceEngine::handleOffer(const std::string& from, const std::vector<uint8_t>& data) {
    LOGI("Handling offer from %s", from.c_str());
    
    // Create peer connection if not exists
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(from);
    if (it == peers_.end()) {
        auto peer = std::make_unique<PeerConnection>(from, this);
        if (!peer->init()) {
            LOGE("Failed to create peer connection");
            return;
        }
        it = peers_.emplace(from, std::move(peer)).first;
    }
    
    it->second->handleOffer(data);
    
    current_peer_id_ = from;
    setConnectionState(ConnectionState::CONNECTING);
}

void VoiceEngine::handleAnswer(const std::string& from, const std::vector<uint8_t>& data) {
    LOGI("Handling answer from %s", from.c_str());
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(from);
    if (it != peers_.end()) {
        it->second->handleAnswer(data);
        setConnectionState(ConnectionState::CONNECTED);
        setCallState(CallState::CONNECTED);
    }
}

void VoiceEngine::handleIceCandidate(const std::string& from, const std::vector<uint8_t>& data) {
    LOGD("Handling ICE candidate from %s", from.c_str());
    
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(from);
    if (it != peers_.end()) {
        it->second->addIceCandidate(data);
    }
}

void VoiceEngine::handleBye(const std::string& from) {
    LOGI("Handling BYE from %s", from.c_str());
    
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto it = peers_.find(from);
        if (it != peers_.end()) {
            it->second->close();
            peers_.erase(it);
        }
    }
    
    if (peer_left_cb_) {
        peer_left_cb_(from);
    }
    
    if (is_private_call_ && from == current_peer_id_) {
        // Call ended
        leaveRoom();
    }
}

void VoiceEngine::handleCallRequest(const std::string& from, const std::vector<uint8_t>& data) {
    LOGI("Incoming call from %s", from.c_str());
    
    // Parse channel ID from data
    std::string channel_id(data.begin(), data.end());
    
    current_peer_id_ = from;
    current_channel_id_ = channel_id;
    is_private_call_ = true;
    setCallState(CallState::RINGING);
    
    if (incoming_call_cb_) {
        incoming_call_cb_(from, channel_id);
    }
}

void VoiceEngine::handleCallAccept(const std::string& from) {
    LOGI("Call accepted by %s", from.c_str());
    
    setCallState(CallState::CONNECTED);
    
    if (call_accepted_cb_) {
        call_accepted_cb_(from);
    }
}

void VoiceEngine::handleCallDecline(const std::string& from, const std::vector<uint8_t>& data) {
    std::string reason(data.begin(), data.end());
    if (reason.empty()) reason = "declined";
    
    LOGI("Call declined by %s: %s", from.c_str(), reason.c_str());
    
    setCallState(CallState::IDLE);
    current_peer_id_.clear();
    current_channel_id_.clear();
    
    if (call_declined_cb_) {
        call_declined_cb_(from, reason);
    }
}

void VoiceEngine::createOffer(const std::string& peer_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        it->second->createOffer();
    }
}

AudioStats VoiceEngine::getAudioStats() const {
    AudioStats stats;
    // TODO: Get actual stats from audio device and peer connections
    stats.latency_ms = 50;  // Placeholder
    stats.packet_loss_percent = 0.0f;
    stats.jitter_ms = 5;
    return stats;
}

bool VoiceEngine::isInRoom() const {
    return !current_channel_id_.empty() || !current_peer_id_.empty();
}

std::vector<std::string> VoiceEngine::getParticipants() const {
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& [id, peer] : peers_) {
        if (peer && peer->isConnected()) {
            result.push_back(id);
        }
    }
    return result;
}

void VoiceEngine::setConnectionState(ConnectionState state) {
    if (connection_state_ != state) {
        connection_state_ = state;
        LOGI("Connection state: %d", static_cast<int>(state));
        if (state_cb_) {
            state_cb_(state);
        }
    }
}

void VoiceEngine::setCallState(CallState state) {
    if (call_state_ != state) {
        call_state_ = state;
        LOGI("Call state: %d", static_cast<int>(state));
    }
}

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<VoiceEngine> g_voice_engine;

VoiceEngine* getVoiceEngine() {
    if (!g_voice_engine) {
        g_voice_engine = std::make_unique<VoiceEngine>();
    }
    return g_voice_engine.get();
}

void destroyVoiceEngine() {
    g_voice_engine.reset();
}

} // namespace grotto::voice
