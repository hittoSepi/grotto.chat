package fi.grotto.android.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class VoiceService : Service() {

    companion object {
        const val CHANNEL_ID = "grotto_voice"
        const val NOTIFICATION_ID = 2
        const val EXTRA_CHANNEL_NAME = "channel_name"
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val channelName = intent?.getStringExtra(EXTRA_CHANNEL_NAME) ?: "Voice"
        val notification = buildNotification(channelName)
        startForeground(NOTIFICATION_ID, notification)
        // TODO: start audio capture/playback via NativeVoice
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        super.onDestroy()
        // TODO: NativeVoice.destroy(), release audio focus
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Voice Call",
            NotificationManager.IMPORTANCE_HIGH
        ).apply {
            description = "Active voice call or room"
        }
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(channel)
    }

    private fun buildNotification(channelName: String): Notification {
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("IrssiCord Voice")
            .setContentText("Voice - $channelName")
            .setSmallIcon(android.R.drawable.ic_btn_speak_now)
            .setOngoing(true)
            .build()
    }
}
