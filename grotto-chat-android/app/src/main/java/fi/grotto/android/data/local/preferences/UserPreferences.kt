package fi.grotto.android.data.local.preferences

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import javax.inject.Inject
import javax.inject.Singleton

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "user_prefs")

@Singleton
class UserPreferences @Inject constructor(
    private val context: Context,
) {
    private val dataStore = context.dataStore

    // Server settings
    val serverAddress: Flow<String?> = dataStore.data.map { it[KEY_SERVER_ADDRESS] }
    val port: Flow<Int> = dataStore.data.map { it[KEY_PORT] ?: DEFAULT_PORT }
    
    // User identity
    val nickname: Flow<String?> = dataStore.data.map { it[KEY_NICKNAME] }
    val identityFingerprint: Flow<String?> = dataStore.data.map { it[KEY_IDENTITY_FINGERPRINT] }
    val identityKeyPair: Flow<ByteArray?> = dataStore.data.map { it[KEY_IDENTITY_KEY_PAIR]?.toByteArray(Charsets.ISO_8859_1) }
    val isRegistered: Flow<Boolean> = dataStore.data.map { it[KEY_IS_REGISTERED] ?: false }
    
    // UI settings
    val themeMode: Flow<String> = dataStore.data.map { it[KEY_THEME_MODE] ?: THEME_SYSTEM }
    val messageStyle: Flow<String> = dataStore.data.map { it[KEY_MESSAGE_STYLE] ?: STYLE_IRC }
    val timestampFormat: Flow<String> = dataStore.data.map { it[KEY_TIMESTAMP_FORMAT] ?: TIMESTAMP_24H }
    val compactMode: Flow<Boolean> = dataStore.data.map { it[KEY_COMPACT_MODE] ?: false }
    val notifyMentions: Flow<Boolean> = dataStore.data.map { it[KEY_NOTIFY_MENTIONS] ?: true }
    val notifyDMs: Flow<Boolean> = dataStore.data.map { it[KEY_NOTIFY_DMS] ?: true }
    val notifySound: Flow<Boolean> = dataStore.data.map { it[KEY_NOTIFY_SOUND] ?: true }
    val pushToTalk: Flow<Boolean> = dataStore.data.map { it[KEY_PUSH_TO_TALK] ?: false }
    val noiseSuppression: Flow<Boolean> = dataStore.data.map { it[KEY_NOISE_SUPPRESSION] ?: true }
    val voiceBitrate: Flow<String> = dataStore.data.map { it[KEY_VOICE_BITRATE] ?: BITRATE_64K }
    val screenCapture: Flow<Boolean> = dataStore.data.map { it[KEY_SCREEN_CAPTURE] ?: false }
    val useTls: Flow<Boolean> = dataStore.data.map { it[KEY_USE_TLS] ?: (it[KEY_PORT] != 6667) }
    val rememberMe: Flow<Boolean> = dataStore.data.map { it[KEY_REMEMBER_ME] ?: false }
    val fontScale: Flow<Float> = dataStore.data.map { it[KEY_FONT_SCALE] ?: FONT_SCALE_NORMAL }
    val linkPreviewsEnabled: Flow<Boolean> = dataStore.data.map { it[KEY_LINK_PREVIEWS] ?: true }

    suspend fun saveServerSettings(address: String, port: Int = DEFAULT_PORT, tls: Boolean? = null) {
        dataStore.edit { prefs ->
            prefs[KEY_SERVER_ADDRESS] = address
            prefs[KEY_PORT] = port
            tls?.let { prefs[KEY_USE_TLS] = it }
        }
    }

    suspend fun saveIdentity(nickname: String, fingerprint: String, keyPair: ByteArray) {
        dataStore.edit { prefs ->
            prefs[KEY_NICKNAME] = nickname
            prefs[KEY_IDENTITY_FINGERPRINT] = fingerprint
            prefs[KEY_IDENTITY_KEY_PAIR] = keyPair.toString(Charsets.ISO_8859_1)
        }
    }

    suspend fun setRegistered(registered: Boolean) {
        dataStore.edit { prefs ->
            prefs[KEY_IS_REGISTERED] = registered
        }
    }

    suspend fun clearIdentity() {
        dataStore.edit { prefs ->
            prefs.remove(KEY_NICKNAME)
            prefs.remove(KEY_IDENTITY_FINGERPRINT)
            prefs.remove(KEY_IDENTITY_KEY_PAIR)
            prefs[KEY_IS_REGISTERED] = false
        }
    }

    /**
     * Save connection settings during setup.
     * These values are used for connecting. The rememberMe flag
     * controls whether they're auto-loaded on next app launch.
     */
    suspend fun saveConnectionSettings(
        address: String,
        port: Int,
        nickname: String,
        useTls: Boolean? = null,
        rememberMe: Boolean,
    ) {
        dataStore.edit { prefs ->
            prefs[KEY_REMEMBER_ME] = rememberMe
            // Always save connection settings (needed for current connection)
            prefs[KEY_SERVER_ADDRESS] = address
            prefs[KEY_PORT] = port
            prefs[KEY_NICKNAME] = nickname
            useTls?.let { prefs[KEY_USE_TLS] = it }
        }
    }

    /**
     * Clear saved connection settings (host, port, username).
     * Does not clear identity or other settings.
     */
    suspend fun clearSavedConnection() {
        dataStore.edit { prefs ->
            prefs.remove(KEY_SERVER_ADDRESS)
            prefs.remove(KEY_PORT)
            prefs.remove(KEY_NICKNAME)
            prefs.remove(KEY_USE_TLS)
            prefs[KEY_REMEMBER_ME] = false
        }
    }

    suspend fun setRememberMe(enabled: Boolean) {
        dataStore.edit { prefs ->
            prefs[KEY_REMEMBER_ME] = enabled
        }
    }

    // UI settings setters
    suspend fun setThemeMode(mode: String) = dataStore.edit { it[KEY_THEME_MODE] = mode }
    suspend fun setMessageStyle(style: String) = dataStore.edit { it[KEY_MESSAGE_STYLE] = style }
    suspend fun setTimestampFormat(format: String) = dataStore.edit { it[KEY_TIMESTAMP_FORMAT] = format }
    suspend fun setCompactMode(enabled: Boolean) = dataStore.edit { it[KEY_COMPACT_MODE] = enabled }
    suspend fun setNotifyMentions(enabled: Boolean) = dataStore.edit { it[KEY_NOTIFY_MENTIONS] = enabled }
    suspend fun setNotifyDMs(enabled: Boolean) = dataStore.edit { it[KEY_NOTIFY_DMS] = enabled }
    suspend fun setNotifySound(enabled: Boolean) = dataStore.edit { it[KEY_NOTIFY_SOUND] = enabled }
    suspend fun setPushToTalk(enabled: Boolean) = dataStore.edit { it[KEY_PUSH_TO_TALK] = enabled }
    suspend fun setNoiseSuppression(enabled: Boolean) = dataStore.edit { it[KEY_NOISE_SUPPRESSION] = enabled }
    suspend fun setVoiceBitrate(bitrate: String) = dataStore.edit { it[KEY_VOICE_BITRATE] = bitrate }
    suspend fun setScreenCapture(enabled: Boolean) = dataStore.edit { it[KEY_SCREEN_CAPTURE] = enabled }
    suspend fun setFontScale(scale: Float) = dataStore.edit { it[KEY_FONT_SCALE] = scale }
    suspend fun setLinkPreviewsEnabled(enabled: Boolean) = dataStore.edit { it[KEY_LINK_PREVIEWS] = enabled }

    companion object {
        const val DEFAULT_PORT = 6697
        
        const val THEME_DARK = "Dark"
        const val THEME_LIGHT = "Light"
        const val THEME_SYSTEM = "System"
        
        const val STYLE_IRC = "IRC"
        const val STYLE_MODERN = "Modern"
        
        const val TIMESTAMP_24H = "HH:MM"
        const val TIMESTAMP_12H = "h:mm A"
        
        const val BITRATE_32K = "32 kbps"
        const val BITRATE_64K = "64 kbps"
        const val BITRATE_96K = "96 kbps"
        const val BITRATE_128K = "128 kbps"
        
        // Font scale options
        const val FONT_SCALE_SMALL = 0.85f
        const val FONT_SCALE_NORMAL = 1.0f
        const val FONT_SCALE_LARGE = 1.15f

        private val KEY_SERVER_ADDRESS = stringPreferencesKey("server_address")
        private val KEY_PORT = intPreferencesKey("port")
        private val KEY_NICKNAME = stringPreferencesKey("nickname")
        private val KEY_IDENTITY_FINGERPRINT = stringPreferencesKey("identity_fingerprint")
        private val KEY_IDENTITY_KEY_PAIR = stringPreferencesKey("identity_key_pair")
        private val KEY_IS_REGISTERED = booleanPreferencesKey("is_registered")
        
        private val KEY_THEME_MODE = stringPreferencesKey("theme_mode")
        private val KEY_MESSAGE_STYLE = stringPreferencesKey("message_style")
        private val KEY_TIMESTAMP_FORMAT = stringPreferencesKey("timestamp_format")
        private val KEY_COMPACT_MODE = booleanPreferencesKey("compact_mode")
        private val KEY_NOTIFY_MENTIONS = booleanPreferencesKey("notify_mentions")
        private val KEY_NOTIFY_DMS = booleanPreferencesKey("notify_dms")
        private val KEY_NOTIFY_SOUND = booleanPreferencesKey("notify_sound")
        private val KEY_PUSH_TO_TALK = booleanPreferencesKey("push_to_talk")
        private val KEY_NOISE_SUPPRESSION = booleanPreferencesKey("noise_suppression")
        private val KEY_VOICE_BITRATE = stringPreferencesKey("voice_bitrate")
        private val KEY_SCREEN_CAPTURE = booleanPreferencesKey("screen_capture")
        private val KEY_USE_TLS = booleanPreferencesKey("use_tls")
        private val KEY_REMEMBER_ME = booleanPreferencesKey("remember_me")
        private val KEY_FONT_SCALE = floatPreferencesKey("font_scale")
        private val KEY_LINK_PREVIEWS = booleanPreferencesKey("link_previews")
    }
}
