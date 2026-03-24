package fi.grotto.android.fcm

import android.content.Context
import androidx.lifecycle.Lifecycle
import android.os.Process
import android.util.Log
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import com.google.firebase.messaging.FirebaseMessaging
import dagger.hilt.android.qualifiers.ApplicationContext
import fi.grotto.android.data.remote.GrottoSocket
import fi.grotto.android.data.remote.ConnectionState
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.tasks.await
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Repository for managing FCM tokens and push notification state.
 * 
 * Responsibilities:
 * - Register/unregister FCM tokens with the Grotto server
 * - Cache tokens locally
 * - Handle retry logic for token registration
 * - Track app foreground/background state
 * - Trigger background sync on wakeup notifications
 */
@Singleton
class FcmRepository @Inject constructor(
    @ApplicationContext private val context: Context,
    private val socket: GrottoSocket,
) : LifecycleObserver {

    private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "fcm_prefs")
    
    private val _tokenState = MutableStateFlow<TokenState>(TokenState.NotRegistered)
    val tokenState: StateFlow<TokenState> = _tokenState.asStateFlow()
    
    private val _isAppInForeground = MutableStateFlow(false)
    val isAppInForeground: StateFlow<Boolean> = _isAppInForeground.asStateFlow()
    
    init {
        // Listen to connection state to retry token registration
        observeConnectionState()
    }
    
    // ============================================================================
    // Token Management
    // ============================================================================
    
    /**
     * Get the current FCM token, or fetch a new one if needed.
     * @return The FCM token, or null if unavailable
     */
    suspend fun getToken(): String? {
        return try {
            FirebaseMessaging.getInstance().token.await()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to get FCM token", e)
            getStoredToken()
        }
    }
    
    /**
     * Request a new FCM token from Firebase.
     * Call this on first app launch or after token deletion.
     */
    suspend fun requestNewToken(): String? {
        return try {
            val token = FirebaseMessaging.getInstance().token.await()
            Log.i(TAG, "New FCM token obtained: ${token.take(16)}...")
            saveTokenLocally(token)
            token
        } catch (e: Exception) {
            Log.e(TAG, "Failed to request FCM token", e)
            null
        }
    }
    
    /**
     * Delete the current FCM token and unregister from server.
     * Call this on logout.
     */
    suspend fun deleteToken() {
        try {
            // Unregister from server first
            val token = getStoredToken()
            if (token != null && socket.connectionState.value == ConnectionState.CONNECTED) {
                unregisterToken(token)
            }
            
            // Delete from Firebase
            FirebaseMessaging.getInstance().deleteToken().await()
            
            // Clear local storage
            clearStoredToken()
            _tokenState.value = TokenState.NotRegistered
            
            Log.i(TAG, "FCM token deleted")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to delete FCM token", e)
        }
    }
    
    /**
     * Register the FCM token with the Grotto server.
     * Should be called when token is refreshed or on login.
     */
    suspend fun registerToken(token: String) {
        _tokenState.value = TokenState.Registering
        
        try {
            // TODO: Send token to Grotto server via protobuf
            // Format: FcmTokenRegistration { string token = 1; string platform = 2; }
            // socket.sendFcmToken(token)
            
            // For now, simulate success
            Log.i(TAG, "Registering FCM token with server: ${token.take(16)}...")
            
            // Wait for server acknowledgment
            // TODO: Implement actual server communication
            // val success = socket.registerFcmToken(token)
            
            // Store token locally
            saveTokenLocally(token)
            _tokenState.value = TokenState.Registered(token)
            
            Log.i(TAG, "FCM token registered successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to register FCM token", e)
            _tokenState.value = TokenState.Error(e.message ?: "Unknown error")
            throw e
        }
    }
    
    /**
     * Unregister the FCM token from the Grotto server.
     * Call this on logout or when disabling notifications.
     */
    suspend fun unregisterToken(token: String) {
        try {
            // TODO: Send unregister request to server
            // socket.unregisterFcmToken(token)
            Log.i(TAG, "Unregistering FCM token from server")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to unregister FCM token", e)
        }
    }
    
    /**
     * Save token locally for later use.
     */
    suspend fun saveTokenLocally(token: String) {
        context.dataStore.edit { prefs ->
            prefs[KEY_FCM_TOKEN] = token
        }
    }
    
    /**
     * Get stored token from local storage.
     */
    suspend fun getStoredToken(): String? {
        return context.dataStore.data.first()[KEY_FCM_TOKEN]
    }
    
    /**
     * Clear stored token from local storage.
     */
    private suspend fun clearStoredToken() {
        context.dataStore.edit { prefs ->
            prefs.remove(KEY_FCM_TOKEN)
        }
    }
    
    /**
     * Check if there's a pending token that needs registration.
     */
    suspend fun checkPendingRegistration() {
        val storedToken = getStoredToken()
        if (storedToken != null && _tokenState.value !is TokenState.Registered) {
            // Try to register the stored token
            try {
                registerToken(storedToken)
            } catch (e: Exception) {
                Log.w(TAG, "Failed to register pending token, will retry on connect")
            }
        }
    }
    
    // ============================================================================
    // Notification Handling
    // ============================================================================
    
    /**
     * Called when a wakeup notification is received.
     * Triggers background sync to fetch new messages.
     */
    suspend fun onWakeupReceived(channelId: String?, senderId: String?, hasMention: Boolean) {
        Log.d(TAG, "Wakeup received: channel=$channelId, sender=$senderId, mention=$hasMention")
        
        // If app is in foreground, just notify the repository to sync
        if (isAppInForeground.value) {
            Log.d(TAG, "App in foreground, triggering sync")
            // TODO: Trigger sync via MessageRepository
            return
        }
        
        // If app is in background, we need to:
        // 1. Connect to server if not connected
        // 2. Fetch new messages
        // 3. Show notifications
        
        if (socket.connectionState.value != ConnectionState.CONNECTED) {
            Log.d(TAG, "App in background, connecting to fetch messages")
            // TODO: Trigger background connection and sync
            // This requires WorkManager for reliable background execution
        }
    }
    
    /**
     * Called when an incoming call notification is received.
     */
    suspend fun onIncomingCall(callerId: String, callerName: String) {
        Log.d(TAG, "Incoming call from $callerName ($callerId)")
        // TODO: Notify VoiceRepository about incoming call
    }
    
    // ============================================================================
    // App Lifecycle
    // ============================================================================
    
    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onAppForeground() {
        Log.d(TAG, "App entered foreground")
        _isAppInForeground.value = true
    }
    
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onAppBackground() {
        Log.d(TAG, "App entered background")
        _isAppInForeground.value = false
    }
    
    fun isAppInForeground(): Boolean {
        return _isAppInForeground.value
    }
    
    // ============================================================================
    // Settings
    // ============================================================================
    
    /**
     * Check if push notifications are enabled.
     */
    val notificationsEnabled: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[KEY_NOTIFICATIONS_ENABLED] ?: true
    }
    
    /**
     * Enable or disable push notifications.
     */
    suspend fun setNotificationsEnabled(enabled: Boolean) {
        context.dataStore.edit { prefs ->
            prefs[KEY_NOTIFICATIONS_ENABLED] = enabled
        }
        
        if (!enabled) {
            // Unsubscribe from topics if needed
            FirebaseMessaging.getInstance().unsubscribeFromTopic("all").await()
        } else {
            // Resubscribe
            FirebaseMessaging.getInstance().subscribeToTopic("all").await()
        }
    }
    
    /**
     * Check if mention notifications are enabled.
     */
    val mentionNotificationsEnabled: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[KEY_MENTION_NOTIFICATIONS] ?: true
    }
    
    suspend fun setMentionNotificationsEnabled(enabled: Boolean) {
        context.dataStore.edit { prefs ->
            prefs[KEY_MENTION_NOTIFICATIONS] = enabled
        }
    }
    
    /**
     * Check if call notifications are enabled.
     */
    val callNotificationsEnabled: Flow<Boolean> = context.dataStore.data.map { prefs ->
        prefs[KEY_CALL_NOTIFICATIONS] ?: true
    }
    
    suspend fun setCallNotificationsEnabled(enabled: Boolean) {
        context.dataStore.edit { prefs ->
            prefs[KEY_CALL_NOTIFICATIONS] = enabled
        }
    }
    
    // ============================================================================
    // Private Methods
    // ============================================================================
    
    private fun observeConnectionState() {
        // When connection is established, retry pending token registration
        // This is handled via coroutine scope in the calling code
    }
    
    companion object {
        private const val TAG = "FcmRepository"
        
        private val KEY_FCM_TOKEN = stringPreferencesKey("fcm_token")
        private val KEY_NOTIFICATIONS_ENABLED = androidx.datastore.preferences.core.booleanPreferencesKey("notifications_enabled")
        private val KEY_MENTION_NOTIFICATIONS = androidx.datastore.preferences.core.booleanPreferencesKey("mention_notifications")
        private val KEY_CALL_NOTIFICATIONS = androidx.datastore.preferences.core.booleanPreferencesKey("call_notifications")
    }
    
    // ============================================================================
    // State Classes
    // ============================================================================
    
    sealed class TokenState {
        object NotRegistered : TokenState()
        object Registering : TokenState()
        data class Registered(val token: String) : TokenState()
        data class Error(val message: String) : TokenState()
    }
}
