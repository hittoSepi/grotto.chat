package fi.grotto.android.domain.model

data class VoiceState(
    val isInVoice: Boolean = false,
    val channelId: String? = null,
    val isMuted: Boolean = false,
    val isDeafened: Boolean = false,
    val participants: List<VoiceParticipant> = emptyList(),
    val isPrivateCall: Boolean = false,
    val callPeerId: String? = null,
    val latencyMs: Int? = null,
)

data class VoiceParticipant(
    val userId: String,
    val isMuted: Boolean = false,
    val isSpeaking: Boolean = false,
    val audioLevel: Float = 0f,
)
