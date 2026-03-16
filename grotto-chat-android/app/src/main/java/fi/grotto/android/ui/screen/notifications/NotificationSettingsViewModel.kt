package fi.grotto.android.ui.screen.notifications

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.fcm.FcmRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * ViewModel for the notification settings screen.
 */
@HiltViewModel
class NotificationSettingsViewModel @Inject constructor(
    private val fcmRepository: FcmRepository,
) : ViewModel() {

    private val _uiState = MutableStateFlow(NotificationSettingsUiState())
    val uiState: StateFlow<NotificationSettingsUiState> = _uiState.asStateFlow()

    init {
        // Collect notification preferences
        combine(
            fcmRepository.notificationsEnabled,
            fcmRepository.mentionNotificationsEnabled,
            fcmRepository.callNotificationsEnabled,
            fcmRepository.tokenState,
        ) { enabled, mentions, calls, tokenState ->
            _uiState.update { state ->
                state.copy(
                    notificationsEnabled = enabled,
                    mentionNotificationsEnabled = mentions,
                    callNotificationsEnabled = calls,
                    fcmToken = (tokenState as? FcmRepository.TokenState.Registered)?.token,
                    isRegistering = tokenState is FcmRepository.TokenState.Registering,
                )
            }
        }.launchIn(viewModelScope)
    }

    /**
     * Enable or disable all notifications.
     */
    fun setNotificationsEnabled(enabled: Boolean) {
        viewModelScope.launch {
            fcmRepository.setNotificationsEnabled(enabled)
            
            if (enabled) {
                // Request and register FCM token
                val token = fcmRepository.requestNewToken()
                if (token != null) {
                    try {
                        fcmRepository.registerToken(token)
                    } catch (e: Exception) {
                        // Will retry on connection
                    }
                }
            } else {
                // Unregister token
                try {
                    fcmRepository.deleteToken()
                } catch (e: Exception) {
                    // Ignore errors during disable
                }
            }
        }
    }

    /**
     * Enable or disable mention notifications.
     */
    fun setMentionNotificationsEnabled(enabled: Boolean) {
        viewModelScope.launch {
            fcmRepository.setMentionNotificationsEnabled(enabled)
        }
    }

    /**
     * Enable or disable call notifications.
     */
    fun setCallNotificationsEnabled(enabled: Boolean) {
        viewModelScope.launch {
            fcmRepository.setCallNotificationsEnabled(enabled)
        }
    }

    /**
     * Manually refresh FCM token.
     */
    fun refreshToken() {
        viewModelScope.launch {
            _uiState.update { it.copy(isRegistering = true) }
            try {
                val token = fcmRepository.requestNewToken()
                if (token != null) {
                    fcmRepository.registerToken(token)
                }
            } catch (e: Exception) {
                // Error state will be updated via tokenState flow
            }
        }
    }
}

/**
 * UI state for notification settings.
 */
data class NotificationSettingsUiState(
    val notificationsEnabled: Boolean = true,
    val mentionNotificationsEnabled: Boolean = true,
    val callNotificationsEnabled: Boolean = true,
    val fcmToken: String? = null,
    val isRegistering: Boolean = false,
    val error: String? = null,
)
