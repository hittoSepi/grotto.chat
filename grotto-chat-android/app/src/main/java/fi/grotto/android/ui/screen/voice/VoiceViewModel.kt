package fi.grotto.android.ui.screen.voice

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.repository.VoiceRepository
import fi.grotto.android.domain.model.VoiceParticipant
import fi.grotto.android.domain.model.VoiceState
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

data class VoiceUiState(
    val channelName: String = "",
    val isEncrypted: Boolean = true,
    val isMuted: Boolean = false,
    val isDeafened: Boolean = false,
    val participants: List<VoiceParticipant> = emptyList(),
    val latencyMs: Int = 0,
    val codec: String = "48kHz Opus",
    val isPrivateCall: Boolean = false,
    val callPeerId: String? = null,
    val isIncoming: Boolean = false,
)

@HiltViewModel
class VoiceViewModel @Inject constructor(
    private val voiceRepository: VoiceRepository,
) : ViewModel() {

    private val _uiState = MutableStateFlow(VoiceUiState())
    val uiState: StateFlow<VoiceUiState> = _uiState.asStateFlow()

    init {
        // Observe voice state from repository
        voiceRepository.voiceState.onEach { voiceState ->
            _uiState.update { state ->
                state.copy(
                    channelName = voiceState.channelId?.let { "#$it" } ?: "",
                    isMuted = voiceState.isMuted,
                    isDeafened = voiceState.isDeafened,
                    participants = voiceState.participants,
                    isPrivateCall = voiceState.isPrivateCall,
                    callPeerId = voiceState.callPeerId,
                    latencyMs = estimateLatency(), // TODO: Get real latency from native layer
                )
            }
        }.launchIn(viewModelScope)
    }

    fun toggleMute() {
        viewModelScope.launch {
            voiceRepository.toggleMute()
        }
    }
    
    fun toggleDeafen() {
        viewModelScope.launch {
            voiceRepository.toggleDeafen()
        }
    }
    
    fun leave() {
        viewModelScope.launch {
            voiceRepository.leaveRoom()
        }
    }
    
    fun acceptCall() {
        viewModelScope.launch {
            voiceRepository.acceptCall()
        }
    }
    
    fun declineCall() {
        viewModelScope.launch {
            voiceRepository.declineCall()
        }
    }
    
    fun startPrivateCall(peerId: String) {
        viewModelScope.launch {
            voiceRepository.call(peerId)
        }
    }
    
    private fun estimateLatency(): Int {
        // TODO: Get real latency measurement from native voice layer
        // For now return a placeholder that varies slightly
        return 20 + (kotlin.random.Random.nextInt(10))
    }
}
