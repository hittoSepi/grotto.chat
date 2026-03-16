package fi.grotto.android.ui.screen.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.security.pinning.CertificatePin
import fi.grotto.android.security.pinning.PinRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import timber.log.Timber
import javax.inject.Inject

/**
 * ViewModel for the certificate pinning screen.
 */
@HiltViewModel
class CertificatePinningViewModel @Inject constructor(
    private val pinRepository: PinRepository,
) : ViewModel() {

    private val _uiState = MutableStateFlow(CertificatePinningUiState())
    val uiState: StateFlow<CertificatePinningUiState> = _uiState.asStateFlow()

    init {
        // Collect pins and enabled state
        combine(
            pinRepository.getAllPins(),
            pinRepository.isPinningEnabled,
        ) { pins, enabled ->
            _uiState.update { state ->
                state.copy(
                    pins = pins,
                    isPinningEnabled = enabled,
                )
            }
        }.launchIn(viewModelScope)
    }

    /**
     * Enable or disable certificate pinning.
     */
    fun setPinningEnabled(enabled: Boolean) {
        viewModelScope.launch {
            pinRepository.setPinningEnabled(enabled)
        }
    }

    /**
     * Add a new certificate pin.
     */
    fun addPin(hostname: String, pin: String, isBackupPin: Boolean) {
        viewModelScope.launch {
            try {
                // Clean up the pin string
                val cleanPin = pin.trim().removePrefix("sha256/")
                pinRepository.addPin(hostname.trim(), cleanPin, isBackupPin)
                Timber.i("Added certificate pin for $hostname")
            } catch (e: Exception) {
                Timber.e(e, "Failed to add certificate pin")
                _uiState.update { it.copy(error = e.message) }
            }
        }
    }

    /**
     * Remove a certificate pin.
     */
    fun removePin(pin: CertificatePin) {
        viewModelScope.launch {
            try {
                pinRepository.removePin(pin.pattern, pin.pin)
                Timber.i("Removed certificate pin for ${pin.pattern}")
            } catch (e: Exception) {
                Timber.e(e, "Failed to remove certificate pin")
                _uiState.update { it.copy(error = e.message) }
            }
        }
    }

    /**
     * Remove all pins for a hostname.
     */
    fun removeAllPinsForHostname(hostname: String) {
        viewModelScope.launch {
            try {
                pinRepository.removeAllPinsForHostname(hostname)
                Timber.i("Removed all pins for $hostname")
            } catch (e: Exception) {
                Timber.e(e, "Failed to remove pins")
                _uiState.update { it.copy(error = e.message) }
            }
        }
    }

    /**
     * Clear all certificate pins.
     */
    fun clearAllPins() {
        viewModelScope.launch {
            try {
                pinRepository.clearAllPins()
                Timber.i("Cleared all certificate pins")
            } catch (e: Exception) {
                Timber.e(e, "Failed to clear pins")
                _uiState.update { it.copy(error = e.message) }
            }
        }
    }

    fun clearError() {
        _uiState.update { it.copy(error = null) }
    }
}

/**
 * UI state for certificate pinning screen.
 */
data class CertificatePinningUiState(
    val pins: List<CertificatePin> = emptyList(),
    val isPinningEnabled: Boolean = true,
    val error: String? = null,
)
