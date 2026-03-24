package fi.grotto.android.ui.screen.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.security.biometric.BiometricAuthManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import timber.log.Timber
import javax.inject.Inject

/**
 * ViewModel for the biometric settings screen.
 */
@HiltViewModel
class BiometricSettingsViewModel @Inject constructor(
    private val biometricAuthManager: BiometricAuthManager,
) : ViewModel() {

    private val _uiState = MutableStateFlow(BiometricSettingsUiState())
    val uiState: StateFlow<BiometricSettingsUiState> = _uiState.asStateFlow()

    init {
        // Load current state
        _uiState.update { state ->
            state.copy(
                isBiometricEnabled = biometricAuthManager.isBiometricEnabled(),
            )
        }
    }

    /**
     * Request to enable biometric authentication.
     * This will trigger the biometric prompt.
     */
    fun requestEnableBiometric() {
        _uiState.update { it.copy(showBiometricPrompt = true) }
    }

    /**
     * Handle the result of the biometric prompt.
     */
    fun onBiometricPromptResult(success: Boolean) {
        _uiState.update { it.copy(showBiometricPrompt = false) }
        
        if (success) {
            setBiometricEnabled(true)
        } else {
            Timber.w("Biometric prompt failed or was cancelled")
        }
    }

    /**
     * Enable or disable biometric authentication.
     */
    fun setBiometricEnabled(enabled: Boolean) {
        biometricAuthManager.setBiometricEnabled(enabled)
        _uiState.update { state ->
            state.copy(
                isBiometricEnabled = enabled,
                message = if (enabled) "Biometric authentication enabled" else "Biometric authentication disabled"
            )
        }
    }

    fun clearMessage() {
        _uiState.update { it.copy(message = null) }
    }
}

/**
 * UI state for biometric settings screen.
 */
data class BiometricSettingsUiState(
    val isBiometricEnabled: Boolean = false,
    val showBiometricPrompt: Boolean = false,
    val message: String? = null,
)
