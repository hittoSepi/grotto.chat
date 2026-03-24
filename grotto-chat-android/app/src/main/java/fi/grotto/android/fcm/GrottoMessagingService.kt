package fi.grotto.android.fcm

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import androidx.core.app.NotificationCompat
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage
import dagger.hilt.android.AndroidEntryPoint
import fi.grotto.android.MainActivity
import fi.grotto.android.R
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import javax.inject.Inject

/**
 * Firebase Cloud Messaging service for Grotto.
 * 
 * Handles:
 * - FCM token registration and updates
 * - Push notification reception
 * - Wakeup notifications for offline messages
 * - Display notifications for new messages
 * 
 * Privacy note: Notifications contain only wakeup signals, not message content.
 * The actual message content is fetched from the server when the app wakes up.
 */
@AndroidEntryPoint
class GrottoMessagingService : FirebaseMessagingService() {

    @Inject
    lateinit var fcmRepository: FcmRepository

    override fun onCreate() {
        super.onCreate()
        createNotificationChannels()
    }

    /**
     * Called when FCM token is updated.
     * Must send the new token to the Grotto server.
     */
    override fun onNewToken(token: String) {
        super.onNewToken(token)
        Log.d(TAG, "FCM token refreshed: ${token.take(16)}...")
        
        CoroutineScope(Dispatchers.IO).launch {
            try {
                fcmRepository.registerToken(token)
                Log.i(TAG, "FCM token registered with server")
            } catch (e: Exception) {
                Log.e(TAG, "Failed to register FCM token", e)
                // Store token locally - will retry on next connection
                fcmRepository.saveTokenLocally(token)
            }
        }
    }

    /**
     * Called when a push message is received.
     * 
     * Message types:
     * - "wakeup" - New messages available, wake up app
     * - "message" - New message notification (title/body only, no content)
     * - "call" - Incoming voice call
     * - "channel_invite" - Invited to a channel
     */
    override fun onMessageReceived(message: RemoteMessage) {
        super.onMessageReceived(message)
        Log.d(TAG, "FCM message received from: ${message.from}")
        
        val data = message.data
        val messageType = data["type"] ?: "wakeup"
        
        when (messageType) {
            "wakeup" -> handleWakeupNotification(data)
            "message" -> handleMessageNotification(data)
            "call" -> handleCallNotification(data)
            "channel_invite" -> handleChannelInviteNotification(data)
            else -> handleGenericNotification(data)
        }
    }

    /**
     * Handle wakeup notification - app should connect to server and fetch messages.
     * No visible notification shown if app is in foreground.
     */
    private fun handleWakeupNotification(data: Map<String, String>) {
        Log.d(TAG, "Handling wakeup notification")
        
        val channelId = data["channel_id"]
        val senderId = data["sender_id"]
        val hasMention = data["has_mention"] == "true"
        
        // Trigger background sync - app will connect and fetch messages
        CoroutineScope(Dispatchers.IO).launch {
            fcmRepository.onWakeupReceived(channelId, senderId, hasMention)
        }
        
        // Only show notification if app is in background and has mention
        if (hasMention && !fcmRepository.isAppInForeground()) {
            showMentionNotification(data)
        }
    }

    /**
     * Handle message notification with title/body.
     * Note: Body contains only preview, not full encrypted message.
     */
    private fun handleMessageNotification(data: Map<String, String>) {
        val channelId = data["channel_id"] ?: return
        val senderName = data["sender_name"] ?: "Someone"
        val channelName = data["channel_name"] ?: "Channel"
        val preview = data["preview"] ?: "New message"
        
        Log.d(TAG, "Message notification: $senderName in $channelName")
        
        showMessageNotification(
            channelId = channelId,
            senderName = senderName,
            channelName = channelName,
            preview = preview
        )
    }

    /**
     * Handle incoming voice call notification.
     */
    private fun handleCallNotification(data: Map<String, String>) {
        val callerId = data["caller_id"] ?: return
        val callerName = data["caller_name"] ?: callerId
        
        Log.d(TAG, "Call notification from: $callerName")
        
        showCallNotification(callerId, callerName)
        
        // Notify VoiceRepository about incoming call
        CoroutineScope(Dispatchers.Main).launch {
            fcmRepository.onIncomingCall(callerId, callerName)
        }
    }

    /**
     * Handle channel invitation notification.
     */
    private fun handleChannelInviteNotification(data: Map<String, String>) {
        val channelId = data["channel_id"] ?: return
        val channelName = data["channel_name"] ?: channelId
        val inviterName = data["inviter_name"] ?: "Someone"
        
        showInviteNotification(channelId, channelName, inviterName)
    }

    /**
     * Handle generic/unknown notification type.
     */
    private fun handleGenericNotification(data: Map<String, String>) {
        val title = data["title"] ?: "Grotto"
        val body = data["body"] ?: "New notification"
        
        showGenericNotification(title, body)
    }

    // ============================================================================
    // Notification Display
    // ============================================================================

    private fun showMessageNotification(
        channelId: String,
        senderName: String,
        channelName: String,
        preview: String
    ) {
        val intent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            putExtra("channel_id", channelId)
        }
        
        val pendingIntent = PendingIntent.getActivity(
            this,
            channelId.hashCode(),
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        
        val notification = NotificationCompat.Builder(this, CHANNEL_MESSAGES)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("$senderName in $channelName")
            .setContentText(preview)
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .setCategory(NotificationCompat.CATEGORY_MESSAGE)
            .build()
        
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(channelId.hashCode(), notification)
    }

    private fun showMentionNotification(data: Map<String, String>) {
        val channelId = data["channel_id"] ?: "general"
        val senderName = data["sender_name"] ?: "Someone"
        val channelName = data["channel_name"] ?: "Channel"
        
        val intent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            putExtra("channel_id", channelId)
            putExtra("highlight", true)
        }
        
        val pendingIntent = PendingIntent.getActivity(
            this,
            (channelId + "_mention").hashCode(),
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        
        val notification = NotificationCompat.Builder(this, CHANNEL_MENTIONS)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("Mentioned by $senderName")
            .setContentText("in $channelName")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .setCategory(NotificationCompat.CATEGORY_MESSAGE)
            .build()
        
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify((channelId + "_mention").hashCode(), notification)
    }

    private fun showCallNotification(callerId: String, callerName: String) {
        val intent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            putExtra("call_from", callerId)
            putExtra("accept_call", true)
        }
        
        val pendingIntent = PendingIntent.getActivity(
            this,
            (callerId + "_call").hashCode(),
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        
        val notification = NotificationCompat.Builder(this, CHANNEL_CALLS)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("Incoming call")
            .setContentText(callerName)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .setCategory(NotificationCompat.CATEGORY_CALL)
            .setFullScreenIntent(pendingIntent, true)
            .build()
        
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify((callerId + "_call").hashCode(), notification)
    }

    private fun showInviteNotification(channelId: String, channelName: String, inviterName: String) {
        val intent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
            putExtra("channel_id", channelId)
            putExtra("invite", true)
        }
        
        val pendingIntent = PendingIntent.getActivity(
            this,
            (channelId + "_invite").hashCode(),
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        
        val notification = NotificationCompat.Builder(this, CHANNEL_MESSAGES)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("Invited to $channelName")
            .setContentText("by $inviterName")
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setContentIntent(pendingIntent)
            .setAutoCancel(true)
            .build()
        
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify((channelId + "_invite").hashCode(), notification)
    }

    private fun showGenericNotification(title: String, body: String) {
        val notification = NotificationCompat.Builder(this, CHANNEL_MESSAGES)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(title)
            .setContentText(body)
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setAutoCancel(true)
            .build()
        
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.notify(System.currentTimeMillis().toInt(), notification)
    }

    // ============================================================================
    // Notification Channels
    // ============================================================================

    private fun createNotificationChannels() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager = getSystemService(NotificationManager::class.java)
            
            // Messages channel
            val messagesChannel = NotificationChannel(
                CHANNEL_MESSAGES,
                "Messages",
                NotificationManager.IMPORTANCE_DEFAULT
            ).apply {
                description = "New messages in channels"
            }
            
            // Mentions channel (higher priority)
            val mentionsChannel = NotificationChannel(
                CHANNEL_MENTIONS,
                "Mentions",
                NotificationManager.IMPORTANCE_HIGH
            ).apply {
                description = "When someone mentions you"
                enableVibration(true)
            }
            
            // Calls channel (highest priority)
            val callsChannel = NotificationChannel(
                CHANNEL_CALLS,
                "Calls",
                NotificationManager.IMPORTANCE_HIGH
            ).apply {
                description = "Incoming voice calls"
                enableVibration(true)
            }
            
            notificationManager.createNotificationChannels(
                listOf(messagesChannel, mentionsChannel, callsChannel)
            )
        }
    }

    companion object {
        private const val TAG = "GrottoFCM"
        
        const val CHANNEL_MESSAGES = "grotto_messages"
        const val CHANNEL_MENTIONS = "grotto_mentions"
        const val CHANNEL_CALLS = "grotto_calls"
    }
}
