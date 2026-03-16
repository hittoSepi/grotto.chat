package fi.grotto.android.data.local.entity

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(
    tableName = "messages",
    indices = [Index("channel_id", "timestamp")]
)
data class MessageEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    @ColumnInfo(name = "channel_id") val channelId: String,
    @ColumnInfo(name = "sender_id") val senderId: String,
    val content: String,
    val timestamp: Long,
    @ColumnInfo(name = "msg_type") val msgType: String = "chat",
    @ColumnInfo(name = "send_status") val sendStatus: String = "sent",
)
