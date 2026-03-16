package fi.grotto.android.ui.screen.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import fi.grotto.android.data.local.preferences.UserPreferences
import fi.grotto.android.data.remote.GrottoSocket
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

data class SettingsUiState(
    val nickname: String = "",
    val identityFingerprint: String = "",
    val serverAddress: String = "",
    val port: String = "6697",
    val isConnected: Boolean = false,
    val themeMode: String = UserPreferences.THEME_SYSTEM,
    val messageStyle: String = UserPreferences.STYLE_IRC,
    val timestampFormat: String = UserPreferences.TIMESTAMP_24H,
    val compactMode: Boolean = false,
    val notifyMentions: Boolean = true,
    val notifyDMs: Boolean = true,
    val notifySound: Boolean = true,
    val pushToTalk: Boolean = false,
    val noiseSuppression: Boolean = true,
    val voiceBitrate: String = UserPreferences.BITRATE_64K,
    val screenCapture: Boolean = false,
    val fontScale: Float = UserPreferences.FONT_SCALE_NORMAL,
    val linkPreviewsEnabled: Boolean = true,
    val version: String = "0.1.0",
    val protocolVersion: String = "v1",
)

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val userPreferences: UserPreferences,
    private val grottoSocket: GrottoSocket,
) : ViewModel() {

    private val _uiState = MutableStateFlow(SettingsUiState())
    val uiState: StateFlow<SettingsUiState> = _uiState.asStateFlow()

    init {
        // Collect all preference flows and update UI state
        combine(
            userPreferences.nickname,
            userPreferences.identityFingerprint,
            userPreferences.serverAddress,
            userPreferences.port,
            grottoSocket.connectionState,
            userPreferences.themeMode,
            userPreferences.messageStyle,
            userPreferences.timestampFormat,
            userPreferences.compactMode,
            userPreferences.notifyMentions,
            userPreferences.notifyDMs,
            userPreferences.notifySound,
            userPreferences.pushToTalk,
            userPreferences.noiseSuppression,
            userPreferences.voiceBitrate,
            userPreferences.screenCapture,
            userPreferences.fontScale,
            userPreferences.linkPreviewsEnabled,
        ) { values ->
            val nickname = values[0] as String? ?: ""
            val fingerprint = values[1] as String? ?: "Not generated"
            val server = values[2] as String? ?: ""
            val port = values[3] as Int
            val connectionState = values[4] as fi.grotto.android.data.remote.ConnectionState
            
            _uiState.update { state ->
                state.copy(
                    nickname = nickname,
                    identityFingerprint = fingerprint,
                    serverAddress = server,
                    port = port.toString(),
                    isConnected = connectionState == fi.grotto.android.data.remote.ConnectionState.CONNECTED,
                    themeMode = values[5] as String,
                    messageStyle = values[6] as String,
                    timestampFormat = values[7] as String,
                    compactMode = values[8] as Boolean,
                    notifyMentions = values[9] as Boolean,
                    notifyDMs = values[10] as Boolean,
                    notifySound = values[11] as Boolean,
                    pushToTalk = values[12] as Boolean,
                    noiseSuppression = values[13] as Boolean,
                    voiceBitrate = values[14] as String,
                    screenCapture = values[15] as Boolean,
                    fontScale = values[16] as Float,
                    linkPreviewsEnabled = values[17] as Boolean,
                )
            }
        }.launchIn(viewModelScope)
    }

    fun setThemeMode(mode: String) {
        viewModelScope.launch { userPreferences.setThemeMode(mode) }
    }
    
    fun setMessageStyle(style: String) {
        viewModelScope.launch { userPreferences.setMessageStyle(style) }
    }
    
    fun setTimestampFormat(format: String) {
        viewModelScope.launch { userPreferences.setTimestampFormat(format) }
    }
    
    fun setCompactMode(v: Boolean) {
        viewModelScope.launch { userPreferences.setCompactMode(v) }
    }
    
    fun setNotifyMentions(v: Boolean) {
        viewModelScope.launch { userPreferences.setNotifyMentions(v) }
    }
    
    fun setNotifyDMs(v: Boolean) {
        viewModelScope.launch { userPreferences.setNotifyDMs(v) }
    }
    
    fun setNotifySound(v: Boolean) {
        viewModelScope.launch { userPreferences.setNotifySound(v) }
    }
    
    fun setPushToTalk(v: Boolean) {
        viewModelScope.launch { userPreferences.setPushToTalk(v) }
    }
    
    fun setNoiseSuppression(v: Boolean) {
        viewModelScope.launch { userPreferences.setNoiseSuppression(v) }
    }
    
    fun setVoiceBitrate(bitrate: String) {
        viewModelScope.launch { userPreferences.setVoiceBitrate(bitrate) }
    }
    
    fun setScreenCapture(v: Boolean) {
        viewModelScope.launch { userPreferences.setScreenCapture(v) }
    }
    
    fun setFontScale(scale: Float) {
        viewModelScope.launch { userPreferences.setFontScale(scale) }
    }
    
    fun setLinkPreviewsEnabled(v: Boolean) {
        viewModelScope.launch { userPreferences.setLinkPreviewsEnabled(v) }
    }
    
    fun disconnect() {
        viewModelScope.launch {
            grottoSocket.disconnect()
        }
    }
    
    fun reconnect() {
        viewModelScope.launch {
            val state = _uiState.value
            if (state.serverAddress.isNotBlank()) {
                grottoSocket.connect(state.serverAddress, state.port.toIntOrNull() ?: 6697)
            }
        }
    }
    
    fun logout() {
        viewModelScope.launch {
            grottoSocket.disconnect()
            userPreferences.clearIdentity()
        }
    }
}
