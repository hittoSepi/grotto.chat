package fi.grotto.android.ndk

import android.util.Log

/**
 * JNI bridge to the shared C++ voice engine (libdatachannel + Opus + Oboe).
 * 
 * This class provides real-time voice communication using WebRTC data channels
 * for transport, Opus for audio encoding, and Oboe for low-latency audio I/O.
 */
object NativeVoice {
    private const val TAG = "NativeVoice"
    
    init {
        try {
            System.loadLibrary("grotto-native")
            Log.i(TAG, "Native library loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load native library", e)
        }
    }
    
    // ============================================================================
    // Data Classes
    // ============================================================================
    
    /**
     * ICE candidate for WebRTC connection establishment.
     */
    data class IceCandidate(
        val sdpMid: String,
        val sdpMLineIndex: Int,
        val candidate: String
    )
    
    /**
     * Session description for WebRTC connection.
     */
    data class SessionDescription(
        val type: String,  // "offer" or "answer"
        val sdp: String
    )
    
    /**
     * Audio statistics for voice connection quality.
     */
    data class AudioStats(
        val latencyMs: Int,
        val packetLossPercent: Float,
        val jitterMs: Int
    )
    
    // ============================================================================
    // Native Methods
    // ============================================================================
    
    /**
     * Initialize the voice engine with audio parameters.
     * @param sampleRate Sample rate in Hz (typically 48000)
     * @param framesPerBuffer Frames per buffer for low latency
     * @return true if initialization successful
     */
    external fun init(sampleRate: Int, framesPerBuffer: Int): Boolean
    
    /**
     * Join a voice room/channel.
     * @param channelId The channel/room identifier
     * @param isPrivateCall true for 1:1 call, false for group voice
     */
    external fun joinRoom(channelId: String, isPrivateCall: Boolean)
    
    /**
     * Leave the current voice room.
     */
    external fun leaveRoom()
    
    /**
     * Initiate a private call to a peer.
     * @param peerId The peer to call
     */
    external fun call(peerId: String)
    
    /**
     * Accept an incoming call.
     */
    external fun acceptCall()
    
    /**
     * Decline an incoming call.
     */
    external fun declineCall()
    
    /**
     * Hang up the current call.
     */
    external fun hangup()
    
    /**
     * Set microphone mute state.
     * @param muted true to mute, false to unmute
     */
    external fun setMuted(muted: Boolean)
    
    /**
     * Set deafen state (don't hear others).
     * @param deafened true to deafen, false to undeafen
     */
    external fun setDeafened(deafened: Boolean)
    
    /**
     * Process incoming WebRTC signaling data.
     * @param fromUser The sender's user ID
     * @param signalType Type of signal (1=offer, 2=answer, 3=ice, 4=bye)
     * @param data The signaling data (SDP or ICE candidate)
     */
    external fun onVoiceSignal(fromUser: String, signalType: Int, data: ByteArray)
    
    /**
     * Send local ICE candidate to peer via signaling.
     * Called by native when new ICE candidate is gathered.
     */
    fun sendIceCandidate(peerId: String, candidate: ByteArray) {
        callback?.onIceCandidate(peerId, candidate)
    }
    
    /**
     * Notify that a peer joined the voice channel.
     * Called by native when peer connection is established.
     */
    fun onPeerJoined(peerId: String) {
        callback?.onPeerJoined(peerId)
    }
    
    /**
     * Notify that a peer left the voice channel.
     * Called by native when peer disconnects.
     */
    fun onPeerLeft(peerId: String) {
        callback?.onPeerLeft(peerId)
    }
    
    /**
     * Update audio level for a peer.
     * Called by native periodically with audio levels.
     */
    fun onAudioLevel(peerId: String, level: Float) {
        callback?.onAudioLevel(peerId, level)
    }
    
    /**
     * Get current audio statistics.
     */
    external fun getAudioStats(): AudioStats
    
    /**
     * Check if currently in a voice room/call.
     */
    external fun isInRoom(): Boolean
    
    /**
     * Get list of participants in current room.
     */
    external fun getParticipants(): Array<String>
    
    /**
     * Cleanup and destroy the voice engine.
     */
    external fun destroy()
    
    // ============================================================================
    // Callback Interface
    // ============================================================================
    
    interface VoiceCallback {
        /**
         * New ICE candidate available to send to peer via signaling server.
         */
        fun onIceCandidate(peerId: String, candidate: ByteArray)
        
        /**
         * A peer joined the voice channel.
         */
        fun onPeerJoined(peerId: String)
        
        /**
         * A peer left the voice channel.
         */
        fun onPeerLeft(peerId: String)
        
        /**
         * Audio level update for a peer (0.0 to 1.0).
         */
        fun onAudioLevel(peerId: String, level: Float)
        
        /**
         * Incoming call received.
         */
        fun onIncomingCall(peerId: String, channelId: String)
        
        /**
         * Call was accepted by peer.
         */
        fun onCallAccepted(peerId: String)
        
        /**
         * Call was declined by peer.
         */
        fun onCallDeclined(peerId: String, reason: String)
        
        /**
         * Connection state changed.
         */
        fun onConnectionStateChanged(state: ConnectionState)
    }
    
    enum class ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        FAILED
    }
    
    // ============================================================================
    // Signal Types
    // ============================================================================
    
    // Signal type constants
    const val SIGNAL_OFFER = 1
    const val SIGNAL_ANSWER = 2
    const val SIGNAL_ICE = 3
    const val SIGNAL_BYE = 4
    const val SIGNAL_CALL_REQUEST = 5
    const val SIGNAL_CALL_ACCEPT = 6
    const val SIGNAL_CALL_DECLINE = 7
    
    private var callback: VoiceCallback? = null
    
    fun setCallback(callback: VoiceCallback?) {
        this.callback = callback
    }
}
