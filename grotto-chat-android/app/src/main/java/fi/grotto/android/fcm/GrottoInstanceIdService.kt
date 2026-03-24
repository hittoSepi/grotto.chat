package fi.grotto.android.fcm

import android.util.Log
import com.google.firebase.messaging.FirebaseMessagingService
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * Service for handling FCM token refresh events.
 * 
 * Note: onNewToken is now in FirebaseMessagingService, so this service
 * is kept for backward compatibility and any future Instance ID events.
 */
@AndroidEntryPoint
class GrottoInstanceIdService : FirebaseMessagingService() {

    @Inject
    lateinit var fcmRepository: FcmRepository

    override fun onNewToken(token: String) {
        super.onNewToken(token)
        Log.d(TAG, "Instance ID token refreshed: ${token.take(16)}...")
        
        // Delegate to repository
        CoroutineScope(Dispatchers.IO).launch {
            try {
                fcmRepository.registerToken(token)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to register refreshed token", e)
                fcmRepository.saveTokenLocally(token)
            }
        }
    }

    companion object {
        private const val TAG = "GrottoInstanceId"
    }
}
