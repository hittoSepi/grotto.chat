#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace grotto::voice {

// ============================================================================
// Forward Declarations
// ============================================================================
class PeerConnection;
class AudioDevice;
struct AudioStats;

// ============================================================================
// Types
// ============================================================================

enum class SignalType : uint8_t {
    OFFER = 1,
    ANSWER = 2,
    ICE_CANDIDATE = 3,
    BYE = 4,
    CALL_REQUEST = 5,
    CALL_ACCEPT = 6,
    CALL_DECLINE = 7
};

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    FAILED
};

enum class CallState {
    IDLE,
    CALLING,
    RINGING,
    CONNECTED,
    ENDED
};

struct AudioStats {
    int latency_ms = 0;
    float packet_loss_percent = 0.0f;
    int jitter_ms = 0;
};

// ============================================================================
// Callbacks
// ============================================================================

using IceCandidateCallback = std::function<void(const std::string& peer_id, 
                                                 const std::vector<uint8_t>& candidate)>;
using PeerJoinedCallback = std::function<void(const std::string& peer_id)>;
using PeerLeftCallback = std::function<void(const std::string& peer_id)>;
using AudioLevelCallback = std::function<void(const std::string& peer_id, float level)>;
using IncomingCallCallback = std::function<void(const std::string& peer_id, 
                                                 const std::string& channel_id)>;
using CallAcceptedCallback = std::function<void(const std::string& peer_id)>;
using CallDeclinedCallback = std::function<void(const std::string& peer_id, 
                                                 const std::string& reason)>;
using ConnectionStateCallback = std::function<void(ConnectionState state)>;

// ============================================================================
// VoiceEngine
// ============================================================================

class VoiceEngine {
public:
    VoiceEngine();
    ~VoiceEngine();
    
    // Non-copyable
    VoiceEngine(const VoiceEngine&) = delete;
    VoiceEngine& operator=(const VoiceEngine&) = delete;
    
    /**
     * Initialize the voice engine.
     * @param sample_rate Sample rate in Hz (typically 48000)
     * @param frames_per_buffer Frames per buffer for low latency
     * @return true if initialization successful
     */
    bool init(int sample_rate, int frames_per_buffer);
    
    /**
     * Cleanup and destroy.
     */
    void destroy();
    
    /**
     * Join a voice room/channel.
     */
    void joinRoom(const std::string& channel_id, bool is_private_call);
    
    /**
     * Leave the current voice room.
     */
    void leaveRoom();
    
    /**
     * Initiate a private call to a peer.
     */
    void call(const std::string& peer_id);
    
    /**
     * Accept an incoming call.
     */
    void acceptCall();
    
    /**
     * Decline an incoming call.
     */
    void declineCall(const std::string& reason = "declined");
    
    /**
     * Hang up the current call.
     */
    void hangup();
    
    /**
     * Set microphone mute state.
     */
    void setMuted(bool muted);
    
    /**
     * Set deafen state.
     */
    void setDeafened(bool deafened);
    
    /**
     * Process incoming WebRTC signaling data.
     */
    void onSignal(const std::string& from_user, SignalType type, 
                  const std::vector<uint8_t>& data);
    
    /**
     * Get current audio statistics.
     */
    AudioStats getAudioStats() const;
    
    /**
     * Check if currently in a voice room.
     */
    bool isInRoom() const;
    
    /**
     * Get list of participants.
     */
    std::vector<std::string> getParticipants() const;
    
    // Callback setters
    void setIceCandidateCallback(IceCandidateCallback cb) { ice_candidate_cb_ = std::move(cb); }
    void setPeerJoinedCallback(PeerJoinedCallback cb) { peer_joined_cb_ = std::move(cb); }
    void setPeerLeftCallback(PeerLeftCallback cb) { peer_left_cb_ = std::move(cb); }
    void setAudioLevelCallback(AudioLevelCallback cb) { audio_level_cb_ = std::move(cb); }
    void setIncomingCallCallback(IncomingCallCallback cb) { incoming_call_cb_ = std::move(cb); }
    void setCallAcceptedCallback(CallAcceptedCallback cb) { call_accepted_cb_ = std::move(cb); }
    void setCallDeclinedCallback(CallDeclinedCallback cb) { call_declined_cb_ = std::move(cb); }
    void setConnectionStateCallback(ConnectionStateCallback cb) { state_cb_ = std::move(cb); }

private:
    bool initialized_ = false;
    int sample_rate_ = 48000;
    int frames_per_buffer_ = 480;
    
    std::string current_channel_id_;
    std::string current_peer_id_;
    bool is_private_call_ = false;
    CallState call_state_ = CallState::IDLE;
    ConnectionState connection_state_ = ConnectionState::DISCONNECTED;
    
    bool muted_ = false;
    bool deafened_ = false;
    
    std::unique_ptr<AudioDevice> audio_device_;
    std::unordered_map<std::string, std::unique_ptr<PeerConnection>> peers_;
    mutable std::mutex peers_mutex_;
    
    // Callbacks
    IceCandidateCallback ice_candidate_cb_;
    PeerJoinedCallback peer_joined_cb_;
    PeerLeftCallback peer_left_cb_;
    AudioLevelCallback audio_level_cb_;
    IncomingCallCallback incoming_call_cb_;
    CallAcceptedCallback call_accepted_cb_;
    CallDeclinedCallback call_declined_cb_;
    ConnectionStateCallback state_cb_;
    
    void setConnectionState(ConnectionState state);
    void setCallState(CallState state);
    void createOffer(const std::string& peer_id);
    void handleOffer(const std::string& from, const std::vector<uint8_t>& data);
    void handleAnswer(const std::string& from, const std::vector<uint8_t>& data);
    void handleIceCandidate(const std::string& from, const std::vector<uint8_t>& data);
    void handleBye(const std::string& from);
    void handleCallRequest(const std::string& from, const std::vector<uint8_t>& data);
    void handleCallAccept(const std::string& from);
    void handleCallDecline(const std::string& from, const std::vector<uint8_t>& data);
};

// ============================================================================
// Global Instance
// ============================================================================

VoiceEngine* getVoiceEngine();
void destroyVoiceEngine();

} // namespace grotto::voice
