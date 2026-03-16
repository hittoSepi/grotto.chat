package fi.grotto.android.ui.screen.auth

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.crypto.NativeCrypto
import fi.grotto.android.crypto.NativeStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.security.MessageDigest
import javax.inject.Inject

data class SetupUiState(
    val serverAddress: String = "",
    val port: String = "6697",
    val nickname: String = "",
    val inviteCode: String = "",
    val rememberMe: Boolean = false,
    val isGenerating: Boolean = false,
    val progress: Float = 0f,
    val currentStep: String = "",
    val steps: List<SetupStep> = listOf(
        SetupStep("Identity key pair", StepStatus.PENDING),
        SetupStep("Signed pre-key", StepStatus.PENDING),
        SetupStep("One-time pre-keys (0/100)", StepStatus.PENDING),
        SetupStep("Saving settings", StepStatus.PENDING),
        SetupStep("Ready to connect", StepStatus.PENDING),
    ),
    val fingerprint: String? = null,
    val error: String? = null,
    val isComplete: Boolean = false,
    val isCleared: Boolean = false,
)

data class SetupStep(val label: String, val status: StepStatus)
enum class StepStatus { PENDING, IN_PROGRESS, DONE }

@HiltViewModel
class SetupViewModel @Inject constructor(
    private val userPreferences: UserPreferences,
    private val nativeStore: NativeStore,
) : ViewModel() {

    private val _uiState = MutableStateFlow(SetupUiState())
    val uiState: StateFlow<SetupUiState> = _uiState.asStateFlow()

    init {
        viewModelScope.launch {
            val rememberMe = userPreferences.rememberMe.first()
            
            // Only load saved values if rememberMe is enabled
            if (rememberMe) {
                val savedAddress = userPreferences.serverAddress.first()
                val savedPort = userPreferences.port.first()
                val savedNickname = userPreferences.nickname.first()

                _uiState.update { state ->
                    state.copy(
                        serverAddress = savedAddress ?: "",
                        port = savedPort.toString(),
                        nickname = savedNickname ?: "",
                        rememberMe = true,
                    )
                }
            }
        }
    }

    fun onServerAddressChanged(value: String) = _uiState.update { it.copy(serverAddress = value) }
    fun onPortChanged(value: String) = _uiState.update { it.copy(port = value) }
    fun onNicknameChanged(value: String) = _uiState.update { it.copy(nickname = value) }
    fun onInviteCodeChanged(value: String) = _uiState.update { it.copy(inviteCode = value) }
    fun onRememberMeChanged(value: Boolean) = _uiState.update { it.copy(rememberMe = value) }

    /**
     * Clear saved connection settings and reset the form.
     */
    fun clearSavedConnection() {
        viewModelScope.launch {
            userPreferences.clearSavedConnection()
            _uiState.update { state ->
                state.copy(
                    serverAddress = "",
                    port = "6697",
                    nickname = "",
                    rememberMe = false,
                    isCleared = true,
                )
            }
            // Reset the cleared flag after a short delay
            delay(100)
            _uiState.update { it.copy(isCleared = false) }
        }
    }



    fun generateKeysAndJoin() {
        viewModelScope.launch {
            val state = _uiState.value
            _uiState.update { it.copy(isGenerating = true, error = null) }

            try {
                // Step 1: Initialize native crypto engine (generates Ed25519 identity key pair)
                updateStep(0, StepStatus.IN_PROGRESS)
                val initOk = withContext(Dispatchers.Default) {
                    NativeCrypto.nativeInit(nativeStore, state.nickname, "")
                }
                if (!initOk) throw RuntimeException("Failed to initialize crypto engine")
                delay(200)
                updateStep(0, StepStatus.DONE)

                // Get fingerprint from the generated identity public key
                val identityPub = NativeCrypto.identityPub()
                    ?: throw RuntimeException("No identity public key")
                val hash = MessageDigest.getInstance("SHA-256").digest(identityPub)
                val fingerprint = hash.take(18).joinToString(":") { "%02X".format(it) }

                // Step 2: Signed pre-key (generated as part of init, just verify)
                updateStep(1, StepStatus.IN_PROGRESS)
                val spk = NativeCrypto.currentSpk()
                    ?: throw RuntimeException("No signed pre-key")
                delay(200)
                updateStep(1, StepStatus.DONE)

                // Step 3: Generate one-time pre-keys via prepareRegistration
                updateStep(2, StepStatus.IN_PROGRESS)
                val uploadBytes = withContext(Dispatchers.Default) {
                    NativeCrypto.prepareRegistration(100)
                } ?: throw RuntimeException("Failed to generate pre-keys")
                delay(200)
                updateStep(2, StepStatus.DONE)

                _uiState.update { s ->
                    val updated = s.steps.toMutableList()
                    updated[2] = updated[2].copy(label = "One-time pre-keys (100/100)")
                    s.copy(steps = updated)
                }

                // Step 4: Save settings
                updateStep(3, StepStatus.IN_PROGRESS)
                val port = state.port.toIntOrNull() ?: 6697
                // Auto-detect TLS: 6667 = plaintext, others = TLS
                val useTls = port != 6667
                userPreferences.saveConnectionSettings(
                    address = state.serverAddress,
                    port = port,
                    nickname = state.nickname,
                    useTls = useTls,
                    rememberMe = state.rememberMe,
                )
                userPreferences.saveIdentity(state.nickname, fingerprint, identityPub)
                delay(200)
                updateStep(3, StepStatus.DONE)

                // Step 5: Mark registered
                updateStep(4, StepStatus.IN_PROGRESS)
                userPreferences.setRegistered(true)
                delay(200)
                updateStep(4, StepStatus.DONE)

                _uiState.update { s ->
                    s.copy(
                        progress = 1f,
                        isGenerating = false,
                        isComplete = true,
                        fingerprint = fingerprint,
                    )
                }
            } catch (e: Exception) {
                _uiState.update {
                    it.copy(
                        isGenerating = false,
                        error = e.message ?: "Unknown error occurred"
                    )
                }
            }
        }
    }

    private fun updateStep(index: Int, status: StepStatus) {
        _uiState.update { state ->
            val updated = state.steps.toMutableList()
            updated[index] = updated[index].copy(status = status)
            state.copy(
                steps = updated,
                progress = (index + 1f) / updated.size
            )
        }
    }
}
