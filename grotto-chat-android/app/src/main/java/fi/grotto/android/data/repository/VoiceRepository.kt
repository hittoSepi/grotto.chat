package fi.grotto.android.data.repository

import android.util.Log
import fi.grotto.android.domain.model.VoiceParticipant
import fi.grotto.android.domain.model.VoiceState
import fi.grotto.android.ndk.NativeVoice
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Repository for managing voice chat state and operations.
 * Uses NativeVoice JNI bridge for real-time voice communication.
 */
@Singleton
class VoiceRepository @Inject constructor() : NativeVoice.VoiceCallback {

    private val _voiceState = MutableStateFlow(VoiceState())
    val voiceState: StateFlow<VoiceState> = _voiceState.asStateFlow()

    init {
        NativeVoice.setCallback(this)
    }

    /**
     * Initialize the voice engine. Should be called once on app startup.
     */
    fun initialize(sampleRate: Int = 48000, framesPerBuffer: Int = 480): Boolean {
        return NativeVoice.init(sampleRate, framesPerBuffer)
    }

    fun joinRoom(channelId: String) {
        NativeVoice.joinRoom(channelId, isPrivateCall = false)
        _voiceState.update { state ->
            state.copy(
                isInVoice = true,
                channelId = channelId,
                isPrivateCall = false,
                callPeerId = null,
            )
        }
    }

    fun leaveRoom() {
        NativeVoice.leaveRoom()
        _voiceState.value = VoiceState()
    }

    fun toggleMute() {
        val newMuted = !_voiceState.value.isMuted
        NativeVoice.setMuted(newMuted)
        _voiceState.update { it.copy(isMuted = newMuted) }
    }
    
    fun toggleDeafen() {
        val newDeafened = !_voiceState.value.isDeafened
        NativeVoice.setDeafened(newDeafened)
        _voiceState.update { it.copy(isDeafened = newDeafened) }
    }
    
    fun call(peerId: String) {
        NativeVoice.call(peerId)
        _voiceState.update { state ->
            state.copy(
                isInVoice = true,
                isPrivateCall = true,
                callPeerId = peerId,
                channelId = null,
            )
        }
    }
    
    fun acceptCall() {
        NativeVoice.acceptCall()
        _voiceState.update { it.copy(isPrivateCall = true) }
    }
    
    fun declineCall() {
        NativeVoice.declineCall()
        _voiceState.update { 
            it.copy(
                isPrivateCall = false,
                callPeerId = null,
            )
        }
    }
    
    fun hangup() {
        NativeVoice.hangup()
        _voiceState.update {
            it.copy(
                isPrivateCall = false,
                callPeerId = null,
            )
        }
    }

    /**
     * Process incoming voice signaling data from the server.
     * @param fromUser The sender's user ID
     * @param signalType Type of signal (see NativeVoice.SIGNAL_* constants)
     * @param data The signaling data
     */
    fun onVoiceSignal(fromUser: String, signalType: Int, data: ByteArray) {
        NativeVoice.onVoiceSignal(fromUser, signalType, data)
    }

    /**
     * Get current audio statistics.
     */
    fun getAudioStats(): NativeVoice.AudioStats {
        return NativeVoice.getAudioStats()
    }

    /**
     * Check if currently in a voice room.
     */
    fun isInRoom(): Boolean {
        return NativeVoice.isInRoom()
    }

    /**
     * Cleanup when app is closing.
     */
    fun destroy() {
        NativeVoice.destroy()
        NativeVoice.setCallback(null)
    }

    // ============================================================================
    // NativeVoice.VoiceCallback Implementation
    // ============================================================================

    override fun onIceCandidate(peerId: String, candidate: ByteArray) {
        // Send ICE candidate to peer via signaling server
        Log.d(TAG, "ICE candidate for $peerId: ${candidate.size} bytes")
        // TODO: Send via GrottoSocket to signaling server
    }

    override fun onPeerJoined(peerId: String) {
        Log.d(TAG, "Peer joined: $peerId")
        _voiceState.update { state ->
            val newParticipants = state.participants + VoiceParticipant(userId = peerId)
            state.copy(participants = newParticipants)
        }
    }

    override fun onPeerLeft(peerId: String) {
        Log.d(TAG, "Peer left: $peerId")
        _voiceState.update { state ->
            val newParticipants = state.participants.filter { it.userId != peerId }
            state.copy(participants = newParticipants)
        }
    }

    override fun onAudioLevel(peerId: String, level: Float) {
        _voiceState.update { state ->
            val newParticipants = state.participants.map { 
                if (it.userId == peerId) it.copy(audioLevel = level, isSpeaking = level > 0.1f) 
                else it 
            }
            state.copy(participants = newParticipants)
        }
    }

    override fun onIncomingCall(peerId: String, channelId: String) {
        Log.d(TAG, "Incoming call from $peerId on channel $channelId")
        _voiceState.update { state ->
            state.copy(
                isInVoice = true,
                isPrivateCall = true,
                callPeerId = peerId,
                channelId = channelId,
            )
        }
    }

    override fun onCallAccepted(peerId: String) {
        Log.d(TAG, "Call accepted by $peerId")
        _voiceState.update { it.copy(isPrivateCall = true) }
    }

    override fun onCallDeclined(peerId: String, reason: String) {
        Log.d(TAG, "Call declined by $peerId: $reason")
        _voiceState.update { 
            it.copy(
                isPrivateCall = false,
                callPeerId = null,
            )
        }
    }

    override fun onConnectionStateChanged(state: NativeVoice.ConnectionState) {
        Log.d(TAG, "Connection state: $state")
        when (state) {
            NativeVoice.ConnectionState.CONNECTED -> {
                // Update audio stats periodically
            }
            NativeVoice.ConnectionState.FAILED -> {
                // Handle connection failure
                leaveRoom()
            }
            else -> {}
        }
    }

    companion object {
        private const val TAG = "VoiceRepository"
    }
}
